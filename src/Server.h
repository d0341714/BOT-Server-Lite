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

     1.0, 20200217

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
     Helen Huang  , helenhuang5566@gmail.com 

 */

#ifndef SERVER_H
#define SERVER_H

#define _GNU_SOURCE

#include "BeDIS.h"
#include "SqlWrapper.h"
#include "GeoFence.h"
#include "HashTable.h"

/* When debugging is needed */
//#define debugging

/* Global variables */

/* Server config file location and the config file definition. */

/* File path of the config file of the server */
#define CONFIG_FILE_NAME "./config/server.conf"

/* File path of the log config file of the server */
#define ZLOG_CONFIG_FILE_NAME "./config/zlog.conf"

/* Maximum length in number of bytes of database information */
#define MAXIMUM_DATABASE_INFO 1024

/* The number of slots in the memory pool for buffer nodes */
#define SLOTS_IN_MEM_POOL_BUFFER_NODE 2048

/* The number of slots in the memory pool for geo-fence area. Each geo-fence
area occupies a slot in this memory pool. */
#define SLOTS_IN_MEM_POOL_GEO_FENCE_AREA 1024

/* The number of slots in the memory pool for geo-fence setting. This memory 
is global for all geo-fence settings in all geo-fence areas. */
#define SLOTS_IN_MEM_POOL_GEO_FENCE_SETTING 2048

/* The number of slots in the memory pool for mac_address of objects 
under geo-fence monitoring. Each geo-fence area occupies a slot in this
memory pool. */
#define SLOTS_IN_MEM_POOL_GEO_FENCE_OBJECTS_SETTING 2048

/* The number of slots in the memory pool for geo-fence violation records. */
#define SLOTS_IN_MEM_POOL_GEO_FENCE_VIOLATIONS 512

/* The number of slots in the memory pool for notification */
#define SLOTS_IN_MEM_POOL_NOTIFICATION 512

/* The maximum number of threads for summarizing object locations */
#define MAX_SUMMARY_TASK_THREADS 256

/* The maximum number of threads for uploading history information */
#define MAX_UPLOAD_HISTORY_TASK_THREADS 256


typedef struct {
    /* The length of the time window in which the movements of an object is 
       monitored. */
    int monitor_interval_in_min;

    /* The time slot in minutes in which the running average of RSSI is 
       calculated to compare with the running average of RSSI in adjacent 
       time slots.
       */
    int each_time_slot_in_min;

    /* The delta value of RSSI larger than which an object is treated 
       as moved. */
    int rssi_delta;

} MovementMonitorConfig;

typedef struct {

   AlarmType alarm_type;

   int alarm_duration_in_sec;

   char gateway_ip[NETWORK_ADDR_LENGTH];

   char agents_list[CONFIG_BUFFER_SIZE];
   
   /* The list entry for inserting the notification entry into the notification 
      list */
   List_Entry notification_list_entry;

} NotificationListNode;

/* The configuration file structure */

typedef struct {
    /* The absoluate file path of server installation path */
    char server_installation_path[MAX_PATH];

    /* The IP address of the server for WiFi netwok connection. */
    char server_ip[NETWORK_ADDR_LENGTH];

    /* The IP address of database for the server to connect. */
    char db_ip[NETWORK_ADDR_LENGTH];

    /* The time interval in seconds between consecutive server requests for 
       health reports from LBeacons */
    int period_between_RFHR;

    /* The time interval in seconds between consecutive server requests for 
       tracked object data from LBeacons */
    int period_between_RFTOD;
    
    /* The time interval in seconds between consecutive checks by the server 
       for object movement information */
    int period_between_check_object_movement_in_sec;

    /* The time difference between server localtime against UTC. For example, 
    this value for servers in Taiwan should be 8, because Taiwan is UTC+8. */
    int server_localtime_against_UTC_in_hour;

    /* A port from which that the server is to send. */
    int send_port;

    /* A port to which gateways are to send to */
    int recv_port;

    /* A port that the database is listening on and the server to send to */
    int database_port;

    /* The name of the database */
    char database_name[MAXIMUM_DATABASE_INFO];

    /* The account for accessing the database */
    char database_account[MAXIMUM_DATABASE_INFO];

    /* The password for accessing the database */
    char database_password[MAXIMUM_DATABASE_INFO];

    /* The number of hours in which data are kept in the database */
    int database_keep_hours;

    /* The number of database connection in the connection pool */
    int number_of_database_connection;

    /* The list head of the database connection pool */
    DBConnectionListHead db_connection_list_head;

    /* The number of threads to do summarizing location information tasks */
    int number_summary_threads;

    /* The number of areas for which each summary thread is responsible */
    int number_areas_per_summary_thread;

    /* The number of threads to do uploading history information tasks */
    int number_upload_history_threads;

    /* The number of areas for which each upload history thread is responsible*/
    int number_areas_per_upload_history_thread;

    /* The RSSI threshold used to filter out tracking data from poor lbeacons*/
    int rssi_threashold_for_summarize_location_pin;

    /* The number of lbeacons to be kept to calculate location of objects */
    int number_of_lbeacons_under_tracked;

    /* The number of rssi signals to be ketp to calculate lcation of objects */
    int number_of_rssi_signals_under_tracked;

    /* The abnormal rssi signal strengh change value. The dramatically rssi 
    signal strengh change will be ignored.*/
    int unreasonable_rssi_change;

    /* The RSSI difference in which the tag is still treated as scanned by
    the strongest and nearest lbeacon*/
    int rssi_difference_of_location_accuracy_tolerance;

    /* The distance tolerance within which the tag is treated as stable
    and its coordinate x and y will not be updated on map. */
    int base_location_tolerance_in_millimeter;

    /* The multiplier used to calcuate the centroid of all scanned lbeacons*/
    int rssi_weight_multiplier;

    /* The time period in which location information of objects are uploaded 
    to time-series database base */
    int time_to_upload_history_location_in_sec;

    /* The flag indicating whether panic button monitor is enabled. */
    int is_enabled_panic_button_monitor;

    /* The flag indicating whether geo-fence monitor is enabled. */
    int is_enabled_geofence_monitor;

    /* The list head of the geo fence list */
    GeoFenceListHead geo_fence_list_head;

    /* The list head of the objects under geo-fence monitoring */
    ObjectWithGeoFenceListHead objects_under_geo_fence_list_head;

    /* The list head of the geo-fence violation records */
    GeoFenceViolationListHead geo_fence_violation_list_head;

    /* The time duration geo-fence perimeter violation remains valid */
    int perimeter_valid_duration_in_sec;

    /* The flag indicating whether location monitor is enabled. */
    int is_enabled_location_monitor;

    /* The flag indicating whether movement monitor is enabled. */
    int is_enabled_movement_monitor;

    /* The specific settings for monitoring movement */
    MovementMonitorConfig movement_monitor_config;
        
    /* The flag indicating collect violation event is enabled. When this flag 
       is set, panic and geo-fence violation events will be collected in 
       notification_table. */
    int is_enabled_collect_violation_event;

    /* The length of the time window in which a violation event in the 
       object_summary_table is treated as valid event.
    */
    int collect_violation_event_time_interval_in_sec;

    /* The length of the time window in which only one representative record 
       of continuous same violations is inserted into notification_table. */
    int granularity_for_continuous_violations_in_sec;

    /* The flag of enable sending notification alarms */
    int is_enabled_send_notification_alarm;

    /* The list head of the notification list */
    struct List_Entry notification_list_head;

    /* The flag indicating whether sending SMS notification is enabled. */
    int is_enabled_send_SMS_notification;

    /* The absoluate file path of SMS notification program */
    char SMS_notification_program_install_path[MAX_PATH];

    /* The mobile phone list to receive SMS notification messages */
    char SMS_contact_list[WIFI_MESSAGE_LENGTH];

    /* The SMS notification message template */
    char SMS_message_template[WIFI_MESSAGE_LENGTH];

} ServerConfig;

/* A server config struct for storing config parameters from the config file */

/* global variables */
ServerConfig config;

/* The database argument for opening database */
char database_argument[SQL_TEMP_BUFFER_LENGTH];

/* The mempool for the Notification node structures */
Memory_Pool notification_mempool;

/* An array of address maps */
AddressMapArray Gateway_address_map;

/* The head of a list of buffers holding message from LBeacons that are parts 
   of GeoFences */
BufferListHead Geo_fence_receive_buffer_list_head;

/* The head of a list of command buffer nodes */
BufferListHead command_buffer_list_head;

/* Variables for storing the last polling times in seconds. Server keeps 
   comparing current MONOTONIC timestamp with these last polling times to 
   determine the timing of periodic polling requests. */
int last_polling_LBeacon_for_HR_time;
int last_polling_object_tracking_time;

/*
  get_server_config:

     This function reads the specified config file line by line until the
     end of file and copies the data in each line into an element of the
     ServerConfig struct global variable.

  Parameters:
     config - Server related configration settings
     common_config - Common configuration settings among gateway and server
     file_name - The name of the config file that stores the server data

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.
                 E_OPEN_FILE: config file  fail to open.
 */

ErrorCode get_server_config(ServerConfig *config, 
                            CommonConfig *common_config, 
                            char *file_name);

/*
  maintain_database:

     This function is executed by a dedicated thread to maintain database 
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

     This function is executed by worker threads on a server when they process 
     the buffer nodes in NSI receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */

void *Server_NSI_routine(void *_buffer_node);

/*
  Server_BHM_routine:

     This function is executed by worker threads on a server when they process 
     the buffer nodes in BHM receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */

void *Server_BHM_routine(void *_buffer_node);


/*
  Server_LBeacon_routine:

     This function is executed by worker threads to process the buffer nodes 
     in data receive buffer list and update the tracking data to database.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */

void *Server_LBeacon_routine(void *_buffer_node);

/*
  process_commands:

     This function is executed by worker threads on a server when processing
     buffer nodes in command buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:
      
     None
*/

void *process_commands(void *_buffer_node);

/*
  process_tracked_data_from_geofence_gateway:

     This function is executed by worker threads on a server when processing
     buffer nodes in the GeoFence receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */

void *process_tracked_data_from_geofence_gateway(void *_buffer_node);

/*
  Gateway_join_request:

     This function is executed on the server in response to a request from a 
     gateway to join the server. When executed, it fills the AddressMap with 
     the inputs and sets the network_address if not exceed MAX_NUMBER_NODES.

  Parameters:

     address_map - The pointer points to the  head of the AddressMap.
     address - The pointer points to the address of the LBeacon IP.
     API_version - API version used by gateway

  Return value:

     bool - true  : Join success.
            false : Fail to join

 */

bool Gateway_join_request(AddressMapArray *address_map, 
                          char *address, 
                          char *API_version);


/*
  broadcast_to_gateway:

     This function is executed when a command needs to be broadcast to gateways.
     When called, this function sends msg to all gateways registered in the
     Gateway_address_map.

  Parameters:
     address_map - The pointer points to the head of the AddressMap.
     msg - The pointer points to the msg to be send to beacons.
     size - The size of the msg.

  Return value:

     None

 */

void broadcast_to_gateway(AddressMapArray *address_map, char *msg, int size);


/*
  Server_process_wifi_send:

     This function sends the message pointed to by an input parameter to all 
     dessinations connected according to the address map via Wi-Fi.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None
 */

void *Server_process_wifi_send(void *_buffer_node);


/*
  Server_process_wifi_receive:

     This function listens for messages or command received from gateways. 
     After getting the message, push the received data into a buffer.

  Parameters:

     None

  Return value:

     None
 */

void *Server_process_wifi_receive();

/*
  Server_monitor_object_violations:

     This function triggers SQL wrapper functions to check if objects violates
     monitoring behaviors.

  Parameters:

     None

  Return value:

     None
 */

void *Server_monitor_object_violations(); 

/*
  Server_reload_monitor_config:

     This function checks if it is time to reload monitoring configurations. If 
     YES, it triggers SQL wrapper functions to reload the settings.

  Parameters:

     None

  Return value:

     None
 */

void *Server_reload_monitor_config(); 

/*
  Server_collect_violation_event:

     This function checks object_summary_table periodically to collect 
     violation events into notification_table.

  Parameters:

     None

  Return value:

     None
 */

void *Server_collect_violation_event(); 


/*
  Server_send_notification:

     This function checks notification_table and sends out notifications.

  Parameters:

     None

  Return value:

     None
 */

void *Server_send_notification(); 


/*
  send_notification_alarm_to_gateway:

     This function is executed when a notification alarm needs to be sent 
     to gateways. When called, this function sends notification alarms to the
     gateways specified in notification settings, and gateways will propagate 
     the notification alarm requests to the agents specified in the request 
     packet.

  Parameters:

  Return value:

     None

 */

void send_notification_alarm_to_gateway();

/*
  add_notification_to_the_notification_list:

     This function parses the configuraion of a notification setting and stores 
     the resultant notification setting struct in the notification setting 
     buffer list.

  Parameters:

     notification_list_head - The pointer to the head of the notification 
                              setting buffer list head.

     buf - The pointer to the buffer containing the configuraion of a 
           notification.

  Return value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.

 */

ErrorCode add_notification_to_the_notification_list(
    struct List_Entry * notification_list_head,
    char *buf);


/*
  summarize_and_upload_location_information_in_areas:

     This function invokes sub-function periodically to upload location 
     information to database table object_summary_table. BOT GUI uses 
     information in this table to display location pin on web page.

  Parameters:

     area_set - the list of areas in which the location information of objects 
                should be summarized and uploaded

  Return value:

     None

 */

void* summarize_and_upload_location_information_in_areas(void *area_set);


/*
  upload_location_history_information_in_areas:

     This function invokes sub-function periodically to upload location 
     history information to database table location_history_table. BOT GUI 
     uses information in this table to display tracking path.

  Parameters:

     area_set - the list of areas in which the location information of objects 
                should be uploaded

  Return value:

     None

 */

void* upload_location_history_information_in_areas(void *area_set);

#endif