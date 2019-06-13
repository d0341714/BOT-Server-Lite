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

     1.0, 20190613

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

   /* The id of the geo fence record in the database */
   int id;

   /* The name of the geo fence record in the database */
   char name[WIFI_MESSAGE_LENGTH];

   /* The list_lock when read or modify the geo fence */
   pthread_mutex_t list_lock;

   /* The list head records the perimeter LBeacon */
   List_Entry perimeters_uuid_list_head;

   /* The list head records the fence LBeacon */
   List_Entry fence_uuid_list_head;

   /* The list head records the prefix of the mac address to be monitored */
   List_Entry mac_prefix_list_head;

   /* The list head records the list of the geo fence */
   List_Entry geo_fence_list_entry;

} sgeo_fence_list_node;

typedef sgeo_fence_list_node *pgeo_fence_list_node;


typedef struct {

   /* The UUID of the LBeacon */
   char uuid[UUID_LENGTH];

   /* The threshold of the LBeacon to check whether the object is in this 
      LBeacon */
   int threshold;
    
   /* The entry of the uuid list */
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

   /* The prefix of the mac address */
   char mac_prefix[LENGTH_OF_MAC_ADDRESS];

   /* The entry of the mac prefix list */
   List_Entry mac_prefix_list_entry;

} smac_prefix_list_node;

typedef smac_prefix_list_node *pmac_prefix_list_node;


typedef struct {

   /* The flag to check the geo fence config is initialized */
   bool is_initialized;

   /* The mempool for geo fence */
   Memory_Pool mempool;

   /* The pointer points to the GeoFence_alert_list_head is used to storing  
    * the alert message */
   BufferListHead *GeoFence_alert_list_head;

   /* The pointer points to the mempool for geo fence alert message
    * This mempool should be manage by the main thread*/
   Memory_Pool *GeoFence_alert_list_node_mempool;

   /* The list head of the geo gence list */
   sgeo_fence_list_node geo_fence_list_head;

   /* The list lock when the geo fence list is in processing */
   pthread_mutex_t geo_fence_list_lock;

} sgeo_fence_config;

typedef sgeo_fence_config *pgeo_fence_config;

/*
  init_geo_fence:

     The function initialize the mempool and worker threads for the Geo-Fence 
     system.

  Parameters:

     geo_fence_config - The pointer points to the geo_fence_config.

     * Need to set the following member of the geo_fence_config before 
       initializing.
         GeoFence_alert_list_node_mempool
         GeoFence_alert_list_head

  Return value:

     ErrorCode

 */
ErrorCode init_geo_fence(pgeo_fence_config geo_fence_config);


/*
  release_geo_fence:

     The function release the mempool and worker threads for the Geo-Fence 
     system.

  Parameters:

     geo_fence_config - The pointer points to the geo_fence_config.

  Return value:

     ErrorCode

 */
ErrorCode release_geo_fence(pgeo_fence_config geo_fence_config);


/*
  geo_fence_check_tracked_object_data_routine:

     This function is executed by worker threads is to filter the tracked 
     object data if the UUID is in monitor.

  Parameters:

     geo_fence_config - The pointer points to the geo fence config.
     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */
ErrorCode geo_fence_check_tracked_object_data_routine(
                                           pgeo_fence_config geo_fence_config,
                                           BufferNode* buffer_node);


/*
  check_geo_fence_routine:

     This function is to filter  mac addresses when the object is not allow to 
     be in the fence or in the perimeter.

  Parameters:

     geo_fence_config - The pointer points to the geo fence config.
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

     geo_fence_config - The pointer points to the geo fence config.
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