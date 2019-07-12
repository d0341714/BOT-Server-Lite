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
#include "Geo-Fencing.h"

/* When debugging is needed */
#define debugging

/* Server config file location and the config file definition. */

/* File path of the config file of the Server */
#define CONFIG_FILE_NAME "./config/server.conf"

/* The category of log file used for health report */
#define LOG_CATEGORY_HEALTH_REPORT "Health_Report"

/* The time interval in seconds for the Server sending the GeoFence table to 
   the GeoFence module (must exceed 1200 seconds) */
#define period_between_update_geo_fence 1200

#ifdef debugging

/* The category of the printf during debugging */
#define LOG_CATEGORY_DEBUG "LBeacon_Debug"

#endif


/* Global variables */

/* The Server config struct for storing config parameters from the config file 
 */
extern ServerConfig serverconfig;

/* The pointer points to the db cursor */
void *Server_db;

/* The struct for storing necessary objects for the Wifi connection */
sudp_config udp_config;

/* The mempool for the buffer node structure to allocate memory */
Memory_Pool node_mempool;

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

/* The struct for storing necessary objects for geo fence */
sgeo_fence_config geo_fence_config;


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
  BHM_routine:

     This function is executed by worker threads when they process the buffer
     nodes in BHM receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *Server_BHM_routine(void *_buffer_node);


/*
  LBeacon_routine:

     This function is executed by worker threads when they process the buffer
     nodes in LBeacon receive buffer list and send to the server directly.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *LBeacon_routine(void *_buffer_node);


/*
  process_GeoFence_routine:

     This function is executed by worker threads when processing
     the buffer node in the GeoFence receive buffer list.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None

 */
void *process_GeoFence_routine(void *_buffer_node);


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
  process_wifi_send:

     This function sends the message in the buffer list to the destination via 
     Wi-Fi.

  Parameters:

     _buffer_node - The pointer points to the buffer node.

  Return value:

     None
 */
void *process_wifi_send(void *_buffer_node);


/*
  process_wifi_receive:

     This function listens for messages or command received from the server or
     LBeacons. After getting the message, push the received data into the 
     buffer.

  Parameters:

     None

  Return value:

     None
 */
void *process_wifi_receive();


#endif