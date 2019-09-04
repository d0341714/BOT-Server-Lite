/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Server.h

  File Description:

     This is the header file containing the declarations of functions and
     variables used in the Server.c file.

  Version:

     1.0, 20190902

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

     Holly Wang   , hollywang@iis.sinica.edu.tw
     Ray Chao     , raychao5566@gmail.com
     Gary Xiao    , garyh0205@hotmail.com
     Chun-Yu Lai  , chunyu1202@gmail.com

 */

#ifndef SERVER_H
#define SERVER_H

#define _GNU_SOURCE

#include "BeDIS.h"
#include "SqlWrapper.h"

/* When debugging is needed */
//#define debugging

/* Global variables */

/* Server config file location and the config file definition. */

/* File path of the config file of the Server */
#define CONFIG_FILE_NAME "./config/server.conf"

/* File path of the config file of the Server */
#define ZLOG_CONFIG_FILE_NAME "./config/zlog.conf"

/* The type name for geo-fence alerts triggered by the presence of monitored 
   objects at LBeacons in a fence. */
#define GEO_FENCE_ALERT_TYPE_FENCE "fence"

/* The type name for geo-fence alerts triggered by the presence of monitored
   objects at LBeacons that are in perimeter of a geofence. */
#define GEO_FENCE_ALERT_TYPE_PERIMETER "perimeter"

/* Length of geo_fence unique key in byte */
#define LENGTH_OF_GEO_FENCE_KEY 32

/* Length of geo_fence name in byte */
#define LENGTH_OF_GEO_FENCE_NAME 32

/* Maximum length for each array of database information */
#define MAXIMUM_DATABASE_INFO 1024

/* Maximum number of LBeacons can be defined as the perimeter in each 
geo-fence rule */
#define MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER 20

/* Maximum number of LBeacons can be defined as the fence in each 
geo-fence rule */
#define MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE 20

/* Maximum number of monitor types can be monitored in each geo-fence rule */
#define MAXIMUM_MONITOR_TYPE_IN_GEO_FENCE 20

typedef struct {
   /* The unique key of the geo fence */
   char unique_key[LENGTH_OF_GEO_FENCE_KEY];

   /* The name of the geo fence */
   char name[LENGTH_OF_GEO_FENCE_NAME];

   int number_perimeters;
   int number_fences;
   
   int rssi_of_perimeters;
   int rssi_of_fences;

   char perimeters[MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER][LENGTH_OF_UUID];
   char fences[MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE][LENGTH_OF_UUID];
   
   /* The list entry for inserting the geofence rule into the list of the geo 
      fences */
   List_Entry geo_fence_list_entry;

} GeoFenceListNode;

typedef struct {
    /* The length of the time window in which an object is shown to be in 
       geofence violation. */
    int geo_fence_time_interval_in_sec;

} GeoFenceMonitorConfig;

typedef struct {
    /* The length of the time window in which an object is monitored for 
       movements.*/
    int time_interval_in_min;

    /* The time slot in minutes in which the running average of RSSI is 
       calculated to compare with the running average of RSSI in adjacent 
       time slots.
       */
    int each_time_slot_in_min;

    /* The delta value of RSSI which an object is treated as moved. */
    int rssi_delta;

} MovementMonitorConfig;

/* The configuration file structure */
typedef struct {

    /* The IP address of the Server for WiFi netwok connection. */
    char server_ip[NETWORK_ADDR_LENGTH];

    /* The IP address of database for the Server to connect. */
    char db_ip[NETWORK_ADDR_LENGTH];

    /* The maximum number of Gateway nodes allowed in the star network of 
    this Server */
    int allowed_number_nodes;

    /* The time interval in seconds between consecutive Server requests for 
       health reports from LBeacons */
    int period_between_RFHR;

    /* The time interval in seconds between consecutive Server requests for 
       tracked object data from LBeacons */
    int period_between_RFTOD;
    
    /* The time interval in seconds between consecutive checks by the Server
       for object location information */
    int period_between_check_object_location;

    /* The time interval in seconds between consecutive checks by the Server 
       for object activity information */
    int period_between_check_object_activity;

    /* A port that gateways are listening on and for the Server to send to. */
    int send_port;

    /* A port that the Server is listening for gateways to send to */
    int recv_port;

    /* A port that the database is listening on and for the Server to send to */
    int database_port;

    /* The name of the database */
    char database_name[MAXIMUM_DATABASE_INFO];

    /* The account for accessing to the database */
    char database_account[MAXIMUM_DATABASE_INFO];

    /* The password for accessing to the database */
    char database_password[MAXIMUM_DATABASE_INFO];

    /* The number of days in which data are kept in the database */
    int database_keep_days;

    /* The length of the time window in which each object is shown and 
       made visiable by BOT system. */
    int location_time_interval_in_sec;

    /* The flag of enable panic button monitor */
    int is_enabled_panic_button_monitor;

    /* The length of the time window in which the object is in the panic 
       condition often the user of the object presses the panic buton of the 
       object. */
    int panic_time_interval_in_sec;

    /* The flag of enable geo-fence monitor */
    int is_enabled_geofence_monitor;

    GeoFenceMonitorConfig geofence_monitor_config;

    /* The flag of enable movement monitor */
    int is_enabled_movement_monitor;

    MovementMonitorConfig movement_monitor_config;
        
    /* The list head of the geo gence list */
    struct List_Entry geo_fence_list_head;

} ServerConfig;

/* A Server config struct for storing config parameters from the config file */

ServerConfig config;

/* The database argument for opening database */
char database_argument[SQL_TEMP_BUFFER_LENGTH];

/* The mempool for the GeoFence node structure to allocate memory */
Memory_Pool geofence_mempool;

/* An array of address maps */
AddressMapArray Gateway_address_map;

/* The head of a list of buffers holding message from LBeacons that are parts 
   of GeoFences */
BufferListHead Geo_fence_receive_buffer_list_head;

/* The head of a list of buffers holding alerts from GeoFences */
BufferListHead Geo_fence_alert_buffer_list_head;

/* Variables for storing the last polling times in seconds. Server keeps 
   comparing current MONOTONIC timestamp with these last polling times to 
   determine the timing of triggering next polling requests periodically. */
int last_polling_LBeacon_for_HR_time;
int last_polling_object_tracking_time;

/*
  get_server_config:

     This function reads the specified config file line by line until the
     end of file and copies the data in each line into an element of the
     ServerConfig struct global variable.

  Parameters:
     config - Server related configration settings
     common_config - Common configuration settings among Gateway and Server
     file_name - The name of the config file that stores the Server data

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.
                 E_OPEN_FILE: config file  fail to open.
 */
ErrorCode get_server_config(ServerConfig *config, 
                            CommonConfig *common_config, 
                            char *file_name);

/*
  maintain_database:

     This function is executed as a dedicated thread to maintain database 
     records by retaining old data from database and doing vacuum on all the 
     tables.

  Parameters:

     None

  Return value:

     None

 */
void *maintain_database();

/*
  Server_NSI_routine:

     This function is executed by worker threads on a Server when they process 
     the buffer nodes in NSI receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_NSI_routine(void *_buffer_node);

/*
  Server_BHM_routine:

     This function is executed by worker threads on a Server when they process 
     the buffer nodes in BHM receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_BHM_routine(void *_buffer_node);


/*
  Server_LBeacon_routine:

     This function is executed by worker threads on a Server when they process 
     the buffer nodes in LBeacon receive buffer list and send to the server 
     directly.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_LBeacon_routine(void *_buffer_node);


/*
  process_tracked_data_from_geofence_gateway:

     This function is executed by worker threads on a Server when processing
     buffer nodes in the GeoFence receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *process_tracked_data_from_geofence_gateway(void *_buffer_node);


/*
  process_GeoFence_alert_routine:

     This function is executed by worker threads on a Server when processing
     buffer nodes in the GeoFence alert buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *process_GeoFence_alert_routine(void *_buffer_node);


/*
  Gateway_join_request:

     This function is executed on the Server in response to a request from a 
     Gateway to join the Server. When executed, it fills the AddressMap with 
     the inputs and sets the network_address if not exceed MAX_NUMBER_NODES.

  Parameters:

     address_map - The pointer points to the  head of the AddressMap.
     address - The pointer points to the address of the LBeacon IP.

  Return value:

     bool - true  : Join success.
            false : Fail to join

 */
bool Gateway_join_request(AddressMapArray *address_map, char *address);


/*
  Broadcast_to_gateway:

     This function is executed when a command needs to be broadcast to Gateways.
     When called, this function sends msg to all Gateways registered in the
     Gateway_address_map.

  Parameters:
     address_map - The pointer points to the head of the AddressMap.
     msg - The pointer points to the msg to be send to beacons.
     size - The size of the msg.

  Return value:

     None

 */
void Broadcast_to_gateway(AddressMapArray *address_map, char *msg, int size);


/*
  Server_process_wifi_send:

     This function sends the message in the buffer list to the destination via 
     Wi-Fi.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None
 */
void *Server_process_wifi_send(void *_buffer_node);


/*
  Server_process_wifi_receive:

     This function listens for messages or command received from the server or
     LBeacons. After getting the message, push the received data into the 
     buffer.

  Parameters:

     None

  Return value:

     None
 */
void *Server_process_wifi_receive();

/*
  Server_summarize_location_information:

     This function checks if it is time to summarize location information of 
     objects. If YES, it triggers SQL wrapper functions to summarize the 
     information.

  Parameters:

     None

  Return value:

     None
 */

void *Server_summarize_location_information(); 

/*
  add_geo_fence_settings:

     This function parses the configuraion of a geo-fence and stores the 
     resultant geo-fence setting struct in the geo-fence setting buffer list.

  Parameters:

     geo_fence_list_head - The pointer to the head of the geo-fence setting 
                           buffer list head.
     buf - The pointer to the buffer containing the configuraion of a 
           geo-fence.

  Return value:

     ErrorCode

 */
ErrorCode add_geo_fence_settings(struct List_Entry * geo_fence_list_head,
                                char *buf);


/*
  check_geo_fence_violations:

     This function checks the object tracking data forwarded by a Geo-Fence 
     Gateway. It compares the tracking data against geo-fence settings in order 
     to detect the objects that violate the geo-fence settings.

  Parameters:

     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */


ErrorCode check_geo_fence_violations(BufferNode* buffer_node);

/*
  insert_into_geo_fence_alert_list:

     This function inserts geo-fence alert log into geo-fence alert buffer 
     list.

  Parameters:

     mac_address - MAC address of detected object.
     fence_type - Type of geo-fence. The possible values being perimeter or 
                  fence.
     lbeacon_uuid - UUID of the LBeacon scanned the detected object.
     final_timestamp - Timestamp at which the lbeacon_uuid scanned this 
                       detected object.
     rssi - RSSI of the detected object seen by lbeacon_uuid.

  Return value:

     ErrorCode

*/

ErrorCode insert_into_geo_fence_alert_list(char *mac_address, 
                                           char *fence_type, 
                                           char *lbeacon_uuid, 
                                           char *final_timestamp, 
                                           char *rssi);


#endif