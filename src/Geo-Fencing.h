/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Geo-Fencing.h

  File Description:

     This header file contains the function declarations and variables used in
     the Geo-Fencing.c file.

  Version:

     1.0, 20190519

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

/* When debugging is needed */
#define debugging


typedef struct {

    int id;

    char name[WIFI_MESSAGE_LENGTH];

    pthread_mutex_t list_lock;

    List_Entry perimeters_uuid_list_head;

    List_Entry fence_uuid_list_head;

    List_Entry mac_prefix_list_head;

    List_Entry geo_fence_list_entry;

} sgeo_fence_list_node;

typedef sgeo_fence_list_node *pgeo_fence_list_node;


typedef struct {

    char uuid[UUID_LENGTH];

    int threshold;

    List_Entry uuid_list_entry;

} suuid_list_node;

typedef suuid_list_node *puuid_list_node;


typedef struct {

    char mac_address[LENGTH_OF_MAC_ADDRESS];

    /* The entry of the mac list */
    List_Entry mac_list_entry;

    /* The entry of the rssi list */
    List_Entry rssi_list_entry;

} stracked_mac_list_node;

typedef stracked_mac_list_node *ptracked_mac_list_node;


typedef struct {

    char mac_prefix[LENGTH_OF_MAC_ADDRESS];

    List_Entry mac_prefix_list_entry;

} smac_prefix_list_node;

typedef smac_prefix_list_node *pmac_prefix_list_node;


typedef struct {

    bool is_running;

    /* The RSSI to determine whether the object is too close to the LBeacon in
       the perimeter or the LBeacon in the fence. */
    int decision_threshold;

    /* UDP for receiving or sending message from and to the BOT server */
    sudp_config udp_config;

    /* The port number which use for receiving message from the BOT server */
    int recv_port;

    /* The port number which is use for the api module to receive message from
       modules such as Geo-Fencing. */
    int api_recv_port;

    char server_ip[NETWORK_ADDR_LENGTH];

    char geo_fence_ip[NETWORK_ADDR_LENGTH];

    /* Worker threads for processing data from the BOT server by assign a
       worker thread. */
    Threadpool worker_thread;

    Memory_Pool pkt_content_mempool;

    Memory_Pool tracked_mac_list_head_mempool;

    Memory_Pool rssi_list_node_mempool;

    Memory_Pool geo_fence_list_node_mempool;

    Memory_Pool uuid_list_node_mempool;

    Memory_Pool mac_prefix_list_node_mempool;

    Memory_Pool geo_fence_mac_prefix_list_node_mempool;

    /* Number of schedule_worker allow to use */
    int number_worker_threads;

    int geo_fence_alert_data_owner_id;

    int geo_fence_data_subscribe_id;

    int tracked_object_data_subscribe_id;

    pthread_t process_api_recv_thread;

    sgeo_fence_list_node geo_fence_list_head;

    pthread_mutex_t geo_fence_list_lock;

} sgeo_fence_config;

typedef sgeo_fence_config *pgeo_fence_config;


typedef struct {

    char ip_address[NETWORK_ADDR_LENGTH];

    char content[WIFI_MESSAGE_LENGTH];

    int content_size;

    pgeo_fence_config geo_fence_config;

} spkt_content;

typedef spkt_content *ppkt_content;


/*
  geo_fence_routine:

     The function is executed by the main thread. Initialize API and sockets for
     the Geo-Fence system.
     * Sockets including UDP sender and receiver.

  Parameters:

     _geo_fence_config - The pointer points to the geo_fence_config.

     * Need to set the following member of the geo_fence_config before starting
       the thread.
            number_worker_threads
            geo_fence_ip
            server_ip
            recv_port
            api_recv_port
            decision_threshold

  Return value:

     NULL

 */
void *geo_fence_routine(void *_geo_fence_config);


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
  check_tracking_object_data_routine:

     This function is executed by worker threads when processing filtering of
     invalid mac addresses.

  Parameters:

     _pkt_content - The pointer points to the pkt content.

  Return value:

     None

 */
static void *check_tracking_object_data_routine(void *_pkt_content);


/*
  update_geo_fence:

     This function is executed by worker threads when receiveing update from the
     bot server.

  Parameters:

     _pkt_content - The pointer points to the pkt content.

  Return value:

     None

 */
static void *update_geo_fence(void *_pkt_content);


/*
  process_api_recv:

     This function is executed by worker threads when receiving msgs from the
     BOT server.

  Parameters:

     _geo_fence_config - The pointer points to the geo fence config.

  Return value:

     None

 */
static void *process_api_recv(void *_geo_fence_config);


/*
  init_geo_fence_list_node:

     This function initialize the geo fence list node by initializing perimeters
     uuid list entry, fence uuid list entry, mac prefix list entry and geo fence
     entry.

  Parameters:

     geo_fence_list_node - The pointer points to the geo fence list node.

  Return value:

     ErrorCode

 */
static ErrorCode init_geo_fence_list_node(
                                      pgeo_fence_list_node geo_fence_list_node);


/*
  free_geo_fence_list_node:

     This function free the geo fence list node by initializing perimeters
     uuid list entry, fence uuid list entry, mac prefix list entry and geo fence
     entry.

  Parameters:

     geo_fence_list_node - The pointer points to the geo fence list node.
     geo_fence_config - The pointer points to the geo fence config.

  Return value:

     ErrorCode

 */
static ErrorCode free_geo_fence_list_node(
                                      pgeo_fence_list_node geo_fence_list_node,
                                      pgeo_fence_config geo_fence_config);


/*
  init_tracked_mac_list_node:

     This function initialize the tracked mac list head by initializing mac list
     entry and lbeacon list head;

  Parameters:

     tracked_mac_list_head - The pointer points to the tracked mac list head.

  Return value:

     ErrorCode

 */
static ErrorCode init_tracked_mac_list_node(ptracked_mac_list_node
                                                         tracked_mac_list_head);


/*
  free_tracked_mac_list_node:

     This function free the tracked mac list head by initializing mac list
     entry and lbeacon list head;

  Parameters:

     tracked_mac_list_head - The pointer points to the tracked mac list head.

  Return value:

     ErrorCode

 */
static ErrorCode free_tracked_mac_list_node(
                                  ptracked_mac_list_node tracked_mac_list_head);


/*
  init_uuid_list_node:

     This function initialize uuid list node by initializing uuid list entry and
     geo fence uuid list entry.

  Parameters:

     uuid_list_node - The pointer points to the uuid list node.

  Return value:

     ErrorCode

 */
static ErrorCode init_uuid_list_node(puuid_list_node uuid_list_node);


/*
  free_uuid_list_node:

     This function free the uuid list node by initializing uuid list entry and
     geo fence uuid list entry.

  Parameters:

     uuid_list_node - The pointer points to the uuid list node.

  Return value:

     ErrorCode

 */
static ErrorCode free_uuid_list_node(puuid_list_node uuid_list_node);


/*
  init_mac_prefix_node:

     This function initialize mac prefix node by initializing mac prefix list
     entry and geo fence mac prefix list node.

  Parameters:

     geo_fence_list_node - The pointer points to the geo fence list node.

  Return value:

     ErrorCode

 */
static ErrorCode init_mac_prefix_node(pmac_prefix_list_node mac_prefix_node);


/*
  free_mac_prefix_node:

     This function free the mac prefix node by initializing mac prefix list
     entry and geo fence mac prefix list node.

  Parameters:

     geo_fence_list_node - The pointer points to the geo fence list node.

  Return value:

     ErrorCode

 */
static ErrorCode free_mac_prefix_node(pmac_prefix_list_node mac_prefix_node);


#endif
