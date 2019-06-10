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

     1.0, 20190606

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

     Chun Yu Lai  , chunyu1202@gmail.com
     Gary Xiao    , garyh0205@hotmail.com

 */

#ifndef GEO_FENCING_H
#define GEO_FENCING_H

#include "BeDIS.h"

/* When debugging is needed */
//#define debugging


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

    bool is_initialized;

    Memory_Pool tracked_mac_list_head_mempool;

    Memory_Pool rssi_list_node_mempool;

    Memory_Pool geo_fence_list_node_mempool;

    Memory_Pool uuid_list_node_mempool;

    Memory_Pool mac_prefix_list_node_mempool;

    Memory_Pool geo_fence_mac_prefix_list_node_mempool;

    BufferListHead *GeoFence_alert_list_head;

    Memory_Pool *GeoFence_alert_list_node_mempool;

    sgeo_fence_list_node geo_fence_list_head;

    pthread_mutex_t geo_fence_list_lock;

} sgeo_fence_config;

typedef sgeo_fence_config *pgeo_fence_config;

emum {
      Perimeter = 1,
      Fence = 2
      };

/*
  init_geo_fence:

     The function initialize the mempool and worker threads for the Geo-Fence 
     system.

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

     ErrorCode

 */
ErrorCode init_geo_fence(pgeo_fence_config geo_fence_config);


/*
  release_geo_fence:

     The function release the mempool and worker threads for the Geo-Fence 
     system.

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

     ErrorCode

 */
ErrorCode release_geo_fence(pgeo_fence_config geo_fence_config);


/*
  geo_fence_check_tracked_object_data_routine:

     This function is executed by worker threads when processing filtering of
     invalid mac addresses.

  Parameters:

     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */
ErrorCode geo_fence_check_tracked_object_data_routine(
                                           pgeo_fence_config geo_fence_config,
                                           int check_type, 
                                           BufferNode* buffer_node);


/*
  check_geo_fence_routine:

     This function is executed by worker threads when processing filtering of
     invalid mac addresses.

  Parameters:

     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */
static ErrorCode check_geo_fence_routine(pgeo_fence_config geo_fence_config, 
                                         BufferNode* buffer_node);


/*
  update_geo_fence:

     This function is executed by worker threads when receiveing update from the
     bot server.

  Parameters:

     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */
ErrorCode update_geo_fence(pgeo_fence_config geo_fence_config, 
                           BufferNode* buffer_node);


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
