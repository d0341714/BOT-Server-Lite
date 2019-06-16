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

     1.0, 20190617

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
    
   /* A string stored all Fences, Perimeters and Mac_pre */
   char fences[WIFI_MESSAGE_LENGTH];

   char perimeters[WIFI_MESSAGE_LENGTH];

   char mac_prefix[WIFI_MESSAGE_LENGTH];

   /* The list head records the list of the geo fence */
   List_Entry geo_fence_list_entry;

} sgeo_fence_list_node;

typedef sgeo_fence_list_node *pgeo_fence_list_node;


typedef struct {

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

     The function initialize the mempool and mutex for the Geo-Fence 
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

     This function is executed by worker threads when processing filtering of
     invalid mac addresses.

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

     This function is executed by worker threads when processing filtering of
     invalid mac addresses.

  Parameters:

     geo_fence_config - The pointer points to the geo fence config.
     geo_fence_list_ptr - The pointer points to the geo fence list node.
     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */
static ErrorCode check_geo_fence_routine(pgeo_fence_config geo_fence_config, 
                                         pgeo_fence_list_node 
                                         geo_fence_list_ptr,
                                         BufferNode *buffer_node);


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

  Return value:

     ErrorCode

 */
static ErrorCode free_geo_fence_list_node(
                                      pgeo_fence_list_node geo_fence_list_node);


#endif