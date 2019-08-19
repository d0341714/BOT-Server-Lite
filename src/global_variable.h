/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     global_variable.h

  File Description:

     This file include the global variables and definition that used in BeDIS 
     and Server but not always commonly used in Gateway and LBeacon.

  Version:

     2.0, 20190718

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

     Jia Ying Shi  , littlestone1225@yahoo.com.tw
 */

#ifndef GLOBAL_VARIABLE_H
#define GLOBAL_VARIABLE_H


/* Length of the LBeacon's UUID in number of characters */
#define LENGTH_OF_UUID 33

/* Length of geo_fence name in byte */
#define LENGTH_OF_GEO_FENCE_NAME 32

/* Number of characters in a Bluetooth MAC address */
#define LENGTH_OF_MAC_ADDRESS 18

/* Maximum length of message to communicate with SQL wrapper API in bytes */
#define SQL_TEMP_BUFFER_LENGTH 4096

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
   /* The name of the geo fence */
   char name[LENGTH_OF_GEO_FENCE_NAME];

   int number_perimeters;
   int number_fences;
   int number_monitor_types;
   
   int rssi_of_perimeters;
   int rssi_of_fences;

   char perimeters[MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER][LENGTH_OF_UUID];
   char fences[MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE][LENGTH_OF_UUID];
   int monitor_types[MAXIMUM_MONITOR_TYPE_IN_GEO_FENCE];
   
   /* The list head records the list of the geo fence */
   List_Entry geo_fence_list_entry;

} GeoFenceListNode;

/* The configuration file structure */
typedef struct {

    /* The IP address of the Server for WiFi netwok connection. */
    char server_ip[NETWORK_ADDR_LENGTH];

    /* The IP address of database for the Server to connect. */
    char db_ip[NETWORK_ADDR_LENGTH];

    /* The maximum number of Gateway nodes allowed in the star network of 
    this Server */
    int allowed_number_nodes;

    /* The time interval in seconds for the Server sending request for health
       reports from LBeacon */
    int period_between_RFHR;

    /* The time interval in seconds for the Server sending request for tracked
       object data from LBeacon */
    int period_between_RFTOD;
    
    /* The time interval in seconds for the Server checking object location 
    information */
    int period_between_check_object_location;

    /* The time interval in seconds for the Server checking object activity 
    information */
    int period_between_check_object_activity;

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

    /*the time window in which we treat this object as shown and visiable by 
    BOT system*/
    int location_time_interval_in_sec;

    /*the time window in which we treat this object as in panic situation if 
    object (user) presses panic button within the interval.*/
    int panic_time_interval_in_sec;

    /*the time window in which we treat this object as shown, visiable, and 
    geofence violation.*/
    int geo_fence_time_interval_in_sec;
    
    /*the time window in which we want to monitor the movement activity.*/
    int inactive_time_interval_in_min;

    /*the time slot in minutes in which we calculate the running average of 
    RSSI for comparison.*/
    int inactive_each_time_slot_in_min;

    /*the delta value of RSSI which we used as a criteria to identify the 
    movement of object.*/
    int inactive_rssi_delta;
    
    /* The list head of the geo gence list */
    struct List_Entry geo_fence_list_head;

} ServerConfig;


/* Global variables */

/* The Server config struct for storing config parameters from the config file 
 */
ServerConfig config;

#endif