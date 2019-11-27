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

/* Maximum number of LBeacons can be defined as the perimeter of each 
   geo-fence */
#define MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER 20

/* Maximum number of LBeacons can be defined as the fence of each 
   geo-fence */
#define MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE 20


typedef struct {
   /* The area id of the geo fence */
   int area_id;
   /* The id of the geo fence in the area id */
   int id;

   /* The name of the geo fence */
   char name[LENGTH_OF_GEO_FENCE_NAME];

   int number_LBeacons_in_perimeter;
   int number_LBeacons_in_fence;
   
   int rssi_of_perimeters;
   int rssi_of_fences;

   /* The flag indicating whether this geo-fence is active at current time according to 
   user's geo-fence monitor configurations. */
   int is_active;

   char perimeters[MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER][LENGTH_OF_UUID];
   char fences[MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE][LENGTH_OF_UUID];
   
   /* The list entry for inserting the geofence into the list of the geo 
      fences */
   List_Entry geo_fence_list_entry;

} GeoFenceListNode;


/*
  add_geo_fence_settings:

     This function parses the configuraion of a geo-fence setting and stores 
     the resultant geo-fence setting struct in the geo-fence setting buffer 
     list.

  Parameters:

     geofence_mempool - The pointer to the memory pool for geo fence setting

     geo_fence_list_head - The pointer to the head of the geo-fence setting 
                           buffer list head.

     buf - The pointer to the buffer containing the configuraion of a 
           geo-fence.

     database_argument - The database argument for opening database 

  Return value:

     ErrorCode

 */

ErrorCode add_geo_fence_settings(Memory_Pool *geofence_mempool,
                                 struct List_Entry * geo_fence_list_head,
                                 char *buf,
                                 char *database_argument);


/*
  check_geo_fence_violations:

     This function first iterates overall enabled and valid geo-fence and then 
     checks if the LBeacon that sent out this message is part of fences. If 
     YES, it invokes helper function examine_tracked_objects_status() to 
     examine each detected object against geo-fence.

  Parameters:

     buffer_node - The pointer points to the buffer node.

     list_head - The pointer to geo fence list head in server global 
                 configuration structure.

     perimeter_valid_duration_in_sec - The time duration geo-fence perimeter 
                                       violation lasts valid 
     
     granularity_for_continuous_violation_in_sec - 
         The length of the time window in which only one representative record 
         of continuous same violations is inserted into notification_table. 
     
     database_argument - The database argument for opening database 

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */

ErrorCode check_geo_fence_violations(BufferNode* buffer_node, 
                                     struct List_Entry *list_head, 
                                     int perimeter_valid_duration_in_sec, 
                                     int granularity_for_continuous_violation_in_sec,
                                     char *database_argument);

/*
  examine_tracked_objects_status:

     This function extracts each detected object data from the buffer and
     checks if the object violates any geo-fence.

  Parameters:

     api_version - API protocol version of BOT_GATEWAY_API used by LBeacon to
                   send out the input message buffer
     
     buf - the packet content sent out by LBeacon

     is_fence_lbeacon - a flag that indicates whether the LBeacon that sent 
                        the data in the input buf is part of a fence

     fence_rssi - the RSSI criteria of fence to determine the detected object 
                  violates fence 

     is_perimeter_lbeacon - a flag that indicates whether the LBeacon that sent 
                            the input buf is part of a perimeter

     perimeter_rssi - the RSSI criteria of perimeter to determine the detected
                      object violates perimeter 

     perimeter_valid_duration_in_sec - The time duration geo-fence perimeter 
                                       violation lasts valid 

     granularity_for_continuous_violation_in_sec - 
         The length of the time window in which only one representative record 
         of continuous same violations is inserted into notification_table. 
     
     database_argument - The database argument for opening database 


  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.

 */

ErrorCode examine_tracked_objects_status(float api_version,
                                         char *buf,
                                         bool is_fence_lbeacon,
                                         int fence_rssi,
                                         bool is_perimeter_lbeacon,
                                         int perimeter_rssi,
                                         int perimeter_valid_duration_in_sec, 
                                         int granularity_for_continuous_violation_in_sec,
                                         char *database_argument);

#endif