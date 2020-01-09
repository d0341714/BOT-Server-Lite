/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     GeoFence.h

  File Description:

     This file contains the header of function declarations and variable used
     in GeoFence.c

  Version:

     1.0, 20191118

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

     Chun-Yu Lai   , chunyu1202@gmail.com
 */

#ifndef GEO_FENCE_H
#define GEO_FENCE_H

#include "BeDIS.h"

/* Length of geo_fence name in byte */
#define LENGTH_OF_GEO_FENCE_NAME 32

/* File path of the dumped active geo-fence settings */
#define DUMP_ACTIVE_GEO_FENCE_FILE "./temp/geo_fence_settings"

/* File path of the dumped mac_address of objects under geo-fence monitor */
#define DUMP_GEO_FENCE_OBJECTS_FILE "./temp/geofence_objects"

/* Number of characters in a geo-fence setting */
#define LENGTH_OF_MAC_ADDRESS_UNDER_GEO_FENCE_MONITOR 4096

/* Number of characters in geo-fence setting */
#define LENGTH_OF_BEACON_UUID_IN_GEO_FENCE_SETTING 4096

/* Type of LBeacon. */
typedef enum _LBeaconType{
    LBEACON_NORMAL = 0,
    LBEACON_PERIMETER = 1,
    LBEACON_FENCE = 2,
} LBeaconType;

typedef struct {

    pthread_mutex_t list_lock;

    struct List_Entry list_head;

} GeoFenceListHead;

typedef struct {

    int area_id;

    List_Entry geo_fence_area_list_entry;

    List_Entry geo_fence_setting_list_head;

} GeoFenceAreaNode;

typedef struct {
   /* The id of the geo fence in the area id */
   int id;

   /* The name of the geo fence */
   char name[LENGTH_OF_GEO_FENCE_NAME];

   int rssi_of_perimeters;
   int rssi_of_fences;

   char perimeters_lbeacon_setting[LENGTH_OF_BEACON_UUID_IN_GEO_FENCE_SETTING];
   char fences_lbeacon_setting[LENGTH_OF_BEACON_UUID_IN_GEO_FENCE_SETTING];

   /* The list entry for inserting the geofence into the list of the geo 
      fences */
   List_Entry geo_fence_setting_list_entry;

} GeoFenceSettingNode;

typedef struct {

    pthread_mutex_t list_lock;

    struct List_Entry list_head;

} ObjectWithGeoFenceListHead;

typedef struct {

    int area_id; 

    char mac_address_under_monitor[LENGTH_OF_MAC_ADDRESS_UNDER_GEO_FENCE_MONITOR];

    List_Entry objects_area_list_entry;

} ObjectWithGeoFenceAreaNode;

typedef struct {

    pthread_mutex_t list_lock;

    struct List_Entry list_head;

} GeoFenceViolationListHead;

typedef struct {

    char mac_address[LENGTH_OF_MAC_ADDRESS];

    int perimeter_violation_timestamp;

    List_Entry geo_fence_violation_list_entry;

} GeoFenceViolationNode;

/* global variables */

/* The mempool for the GeoFence area node structures */
Memory_Pool geofence_area_mempool;

/* The mempool for the GeoFence setting node structures */
Memory_Pool geofence_setting_mempool;

/* The mempool for the mac_address of objects under geo-fence monitor structures */
Memory_Pool geofence_objects_area_mempool;

/* The mempool for the geo-fence violation structures */
Memory_Pool geofence_violation_mempool;

/*
  construct_geo_fence_list:

     This function constructs the geo-fence setting.

  Parameters:

     db_connection_list_head - the list head of database connection pool
     
     geo_fence_list_head - The pointer to geo fence list head in server global 
                           configuration structure.

     is_to_all_areas - The flag indicating whether to construct geo fence list in 
                       all covered areas

     target_area_id - The specific targeted area_id. This argument takes effects
                      only when the input argument is_to_all_areas is false

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */
ErrorCode construct_geo_fence_list(DBConnectionListHead *db_connection_list_head,
                                   GeoFenceListHead* geo_fence_list_head,
                                   bool is_to_all_areas,
                                   int target_area_id);

/*
  destroy_geo_fence_list:

     This function destroy the geo-fence setting.

  Parameters:

     geo_fence_list_head - The pointer to geo fence list head in server global 
                           configuration structure.

     is_to_all_areas - The flag indicating whether to destory geo fence list in 
                       all covered areas

     target_area_id - The specific targeted area_id. This argument takes effects 
                      only when the input argument is_to_all_areas is false

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */
ErrorCode destroy_geo_fence_list(GeoFenceListHead* geo_fence_list_head,
                                 bool is_to_all_areas,
                                 int target_area_id);

/*
  construct_objects_list_under_geo_fence_monitoring:

     This function constructs the objects under geo-fence monitoring.

  Parameters:

     db_connection_list_head - the list head of database connection pool
     
     objects_list_head - The pointer to list of objects under geo-fence 
                         monitoring

     is_to_all_areas - The flag indicating whether to construct the list of 
                       objects under geo fence monitoring in all covered areas

     target_area_id - The specific targeted area_id. This argument takes effects
                      only when the input argument is_to_all_areas is false

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */
ErrorCode construct_objects_list_under_geo_fence_monitoring(
    DBConnectionListHead * db_connection_list_head,
    ObjectWithGeoFenceListHead* objects_list_head,
    bool is_to_all_areas,
    int target_area_id);

/*
  destroy_objects_list_under_geo_fence_monitoring:

     This function destroy the objects under geo-fence monitoring.

  Parameters:

     objects_list_head - The pointer to list of objects under geo-fence 
                         monitoring

     is_to_all_areas - The flag indicating whether to destroy the list of 
                       objects under geo fence monitoring in all covered areas

     target_area_id - The specific targeted area_id. This argument takes effects
                      only when the input argument is_to_all_areas is false
                        
  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */
ErrorCode destroy_objects_list_under_geo_fence_monitoring(
    ObjectWithGeoFenceListHead* objects_list_head,
    bool is_to_all_areas,
    int target_area_id);

/* 
  reload_geo_fence_settings:

      This function reload geo fence settings according to information specified 
      in the input command_buf argument.

  Parameters:

      command_buf - The pointers to command buffer specified by user

      db_connection_list_head - the list head of database connection pool

      geo_fence_list_head - The pointers to geo fence list head in server global 
                            configuration structures

      objects_list_head - The pointer to list of objects under geo-fence 
                          monitoring
*/

ErrorCode reload_geo_fence_settings(char *command_buf,
                                    DBConnectionListHead *db_connection_list_head,
                                    GeoFenceListHead * geo_fence_list_head,
                                    ObjectWithGeoFenceListHead * objects_list_head);
/*
  check_geo_fence_violations:

     This function first iterates overall enabled and valid geo-fence and then 
     checks if the LBeacon that sent out this message is part of fences. If 
     YES, it invokes helper function examine_tracked_objects_status() to 
     examine each detected object against geo-fence.

  Parameters:

     buffer_node - The pointer points to the buffer node.

     db_connection_list_head - the list head of database connection pool

     geo_fence_list_head - The pointer to geo fence list head in server global 
                           configuration structure.

     objects_list_head - The pointer to list of objects under geo-fence monitoring

     geo_fence_violation_list_head - The pointer to list of geo-fence violation 
                                     records

     perimeter_valid_duration_in_sec - The time duration geo-fence perimeter 
                                       violation lasts valid 
     
  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */

ErrorCode check_geo_fence_violations(
    BufferNode* buffer_node, 
    DBConnectionListHead *db_connection_list_head,
    GeoFenceListHead* geo_fence_list_head,
    ObjectWithGeoFenceListHead * objects_list_head,
    GeoFenceViolationListHead * geo_fence_violation_list_head,
    int perimeter_valid_duration_in_sec);


/*
  examine_object_tracking_data:

     This function examine tracking data buffer content to compare against 
     geo-fence settings. 

  Parameters:

     buffer_node - The pointer points to the buffer node.

     area_id - The area id in which the lbeacon uuid within buffer specifies

     lbeacon_type - a flag indicating whether the lbeacon is part of perimeter or 
                    fence

     rssi_criteria - the rssi strenth to determine if objects are closed to lbeacon

     objects_list_head - The pointer to list of objects under geo-fence monitoring

     geo_fence_violation_list_head - The pointer to list of geo-fence violation 
                                     records

     perimeter_valid_duration_in_sec - The time duration geo-fence perimeter 
                                       violation lasts valid 
     
     db_connection_list_head - the list head of database connection pool

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */

ErrorCode examine_object_tracking_data(
    BufferNode *buffer_node,
    int area_id,
    LBeaconType lbeacon_type,
    int rssi_criteria,
    ObjectWithGeoFenceListHead * objects_list_head,
    GeoFenceViolationListHead * geo_fence_violation_list_head,
    int perimeter_valid_duration_in_sec,
    DBConnectionListHead *db_connection_list_head);

#endif