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

     Holly Wang   , hollywang@iis.sinica.edu.tw
     Ray Chao     , raychao5566@gmail.com
     Gary Xiao    , garyh0205@hotmail.com
     Chun Yu Lai  , chunyu1202@gmail.com

 */

#ifndef SERVER_H
#define SERVER_H

#define _GNU_SOURCE

#include "BeDIS.h"
#include "SqlWrapper.h"

/* When debugging is needed */
#define debugging

/* Server config file location and the config file definition. */

/* File path of the config file of the Server */
#define CONFIG_FILE_NAME "./config/server.conf"

/* File path of the config file of the Server */
#define ZLOG_CONFIG_FILE_NAME ".\\config\\zlog.conf"

/* The category of log file used for health report */
#define LOG_CATEGORY_HEALTH_REPORT "Health_Report"

/* The category of the printf during debugging */
#define LOG_CATEGORY_DEBUG "LBeacon_Debug"



typedef struct {
   /* The name of the geo fence */
   char name[LENGTH_OF_GEO_FENCE_NAME];

   int number_perimeters;
   int number_fences;
   int number_mac_prefixes;

   int rssi_of_perimeters;
   int rssi_of_fences;

   char perimeters[20][LENGTH_OF_UUID];
   char fences[20][LENGTH_OF_UUID];
   char mac_prefixes[20][LENGTH_OF_MAC_ADDRESS];

   /* The list head records the list of the geo fence */
   List_Entry geo_fence_list_entry;

} GeoFenceListNode;



/* The configuration file structure */
typedef struct {

    /* The IP address of the Server for WiFi netwok connection. */
    char server_ip[NETWORK_ADDR_LENGTH];

    /* The IP address of database for the Server to connect. */
    char db_ip[NETWORK_ADDR_LENGTH];

    /* The maximum number of Gateway nodes allowed in the star network of this Server */
    int allowed_number_nodes;

    /* The time interval in seconds for the Server sending request for health
       reports from LBeacon */
    int period_between_RFHR;

    /* The time interval in seconds for the Server sending request for tracked
       object data from LBeacon */
    int period_between_RFTOD;

    /* The number of worker threads used by the communication unit for sending
      and receiving packets to and from LBeacons and the sever. */
    int number_worker_threads;

    /* A port that Gateways are listening on and for the Server to send to. */
    int send_port;

    /* A port that the Server is listening for Gateways to send to */
    int recv_port;

    /* A port that the database is listening on for the Server to send to */
    int database_port;

    /* The name of the database */
    char database_name[MAXIMUM_DATABASE_INFO];

    /* The account for accessing to the database */
    char database_account[MAXIMUM_DATABASE_INFO];

    /* The password for accessing to the database */
    char database_password[MAXIMUM_DATABASE_INFO];

    /* The number of days in which we want to keep the data in the database */
    int database_keep_days;

    /* Priority levels at which buffer lists are processed by the worker threads
     */
    int time_critical_priority;
    int high_priority;
    int normal_priority;
    int low_priority;

    /* The list head of the geo gence list */
    struct List_Entry geo_fence_list_head;

} ServerConfig;

/* Global variables */

/* The Server config struct for storing config parameters from the config file 
 */
ServerConfig serverconfig;

/* The pointer points to the db cursor */
void *Server_db;

/* The struct for storing necessary objects for the Wifi connection */
sudp_config udp_config;

/* The mempool for the buffer node structure to allocate memory */
Memory_Pool node_mempool;

/* The mempool for the GeoFence node structure to allocate memory */
Memory_Pool geofence_mempool;

/* An array of address maps */
AddressMapArray Gateway_address_map;

/* The head of a list of buffers of data from LBeacons */
BufferListHead LBeacon_receive_buffer_list_head;

/* The head of a list of the return message for the Gateway join requests */
BufferListHead NSI_send_buffer_list_head;

/* The head of a list of buffers for return join request status */
BufferListHead NSI_receive_buffer_list_head;

/* The head of a list of buffers holding health reports to be processed and sent
   to the Server */
BufferListHead BHM_send_buffer_list_head;

/* The head of a list of buffers holding health reports from LBeacons */
BufferListHead BHM_receive_buffer_list_head;

/* The head of a list of buffers holding message from LBeacons specified by 
   GeoFence */
BufferListHead Geo_fence_receive_buffer_list_head;

/* The head of a list of buffers holding GeoFence alert from GeoFence */
BufferListHead Geo_fence_alert_buffer_list_head;

/* The head of a list of buffers for the buffer list head in the priority 
   order. */
BufferListHead priority_list_head;


/* Flags */

/*
  Initialization of the Server components involves network activates that may
  take time. These flags enable each module to inform the main thread when its
  initialization completes.
 */
bool NSI_initialization_complete;
bool CommUnit_initialization_complete;

/* The flag is to identify whether any component fail to initialize */
bool initialization_failed;

/* Variables for storing the last polling times in second*/
int last_polling_LBeacon_for_HR_time;
int last_polling_object_tracking_time;

/* Variables for storing the last updating times in second*/
int last_update_geo_fence;


/*
  get_server_config:

     This function reads the specified config file line by line until the
     end of file and copies the data in each line into an element of the
     ServerConfig struct global variable.

  Parameters:

     file_name - The name of the config file that stores the Server data

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.
                 E_OPEN_FILE: config file  fail to open.
 */
ErrorCode get_server_config(ServerConfig *config, char *file_name);


/*
  sort_priority_list:

     The function arrange entries in the priority list in nonincreasing
     order of the priority nice.

  Parameters:

     config - The pointer points to the structure which stored config for
              gateway.
     list_head - The pointer points to the priority list head.

  Return value:

     None
 */
void *sort_priority_list(ServerConfig *config, BufferListHead *list_head);


/*
  CommUnit_routine:

     The function is executed by the main thread of the communication unit that
     is responsible for sending and receiving packets to and from the sever and
     LBeacons after the NSI module has initialized WiFi networks. It creates
     threads to carry out the communication process.

  Parameters:

     None

  Return value:

     None

 */
void *CommUnit_routine();


/*
  maintain_database:

     This function is executed as a dedicated thread and it maintains database 
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

     This function is executed by worker threads when they process the buffer
     nodes in NSI receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_NSI_routine(void *_buffer_node);

/*
  Server_BHM_routine:

     This function is executed by worker threads when they process the buffer
     nodes in BHM receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_BHM_routine(void *_buffer_node);


/*
  Server_LBeacon_routine:

     This function is executed by worker threads when they process the buffer
     nodes in LBeacon receive buffer list and send to the server directly.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_LBeacon_routine(void *_buffer_node);


/*
  process_tracked_data_from_geofence_gateway:

     This function is executed by worker threads when processing
     the buffer node in the GeoFence receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *process_tracked_data_from_geofence_gateway(void *_buffer_node);


/*
  process_GeoFence_alert_routine:

     This function is executed by worker threads when processing
     the buffer node in the GeoFence alert buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *process_GeoFence_alert_routine(void *_buffer_node);


/*
  Gateway_join_request:

     This function is executed when a Gateway sends a command to join the Server
     . When executed, it fills the AddressMap with the inputs and sets the
     network_address if not exceed MAX_NUMBER_NODES.

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
  add_geo_fence_setting:

     This function parses one configuraion of geo-fence and stores the 
     geo-fence setting structure into the geo-fence setting buffer list.

  Parameters:

     geo_fence_list_head - The pointer points to the geo-fence setting 
                           buffer list head.
     buf - The pointer points to the buffer containing one configuraion of 
           geo-fence setting. 

  Return value:

     ErrorCode

 */
ErrorCode add_geo_fence_setting(struct List_Entry * geo_fence_list_head,
                                char *buf);


/*
  detect_geo_fence_violations:

     This function checks the object tracking data forwarded by Geo-Fence 
     Gateway. It compares the tracking data against geo-fence settings and 
     detects if the objects violates the geo-fence settings.

  Parameters:

     buffer_node - The pointer points to the buffer node.

  Return value:

     ErrorCode

 */


ErrorCode check_geo_fence_violations(BufferNode* buffer_node);

/*
  insert_into_geo_fence_alert_list:

     This function inserts geo-fence alert log into geo-fence alert buffer 
     list

  Parameters:

     mac_address - MAC address of detected object.
     fence_type - Type of geo-fence setting. The possible values are 
                  perimeter and fence.
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