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

     1.0, 20190308

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

#ifndef GATEWAY_H
#define GATEWAY_H

#define _GNU_SOURCE

#include "BeDIS.h"
#include "SqlWrapper.h"
#include "BOT_API.h"

/* When debugging is needed */
#define debugging

/* Server config file location and the config file definition. */

/* File path of the config file of the Server */
#define CONFIG_FILE_NAME "./config/server.conf"

/* The category of log file used for health report */
#define LOG_CATEGORY_HEALTH_REPORT "Health_Report"

#define MAXIMUM_DATABASE_INFO 1024

#ifdef debugging

/* The category of the printf during debugging */
#define LOG_CATEGORY_DEBUG "LBeacon_Debug"

#endif

/* Maximum number of nodes (Gateways) per star network rooted at a Server */
#define MAX_NUMBER_NODES 32

#define TEST_MALLOC_MAX_NUMBER_TIMES 5

/*
  Maximum length of time in seconds low priority message lists are starved
  of attention. */
#define MAX_STARVATION_TIME 600


/* The configuration file structure */
typedef struct {

    /* The IP address of the Server for WiFi netwok connection. */
    char server_ip[NETWORK_ADDR_LENGTH];

    /* The IP address of database for the Server to connect. */
    char db_ip[NETWORK_ADDR_LENGTH];

    /* The number of Gateway nodes in the star network of this Server */
    int allowed_number_nodes;

    /* The time interval in seconds for the Server sending request for health
       reports from LBeacon */
    int period_between_RFHR;

    /* The time interval in seconds for the Server sending request for tracked
       object data from LBeacon */
    int period_between_RFTOD;

    /*The number of worker threads used by the communication unit for sending
      and receiving packets to and from LBeacons and the sever.*/
    int number_worker_threads;

    /* A port that Gateways are listening on and for the Server to send to. */
    int send_port;

    /* A port that the Server is listening for Gateways to send to */
    int recv_port;

    /* A port that the database is listening on for the Server to send to */
    int database_port;

    char database_name[MAXIMUM_DATABASE_INFO];

    char database_account[MAXIMUM_DATABASE_INFO];

    char database_password[MAXIMUM_DATABASE_INFO];

    /* Priority levels at which buffer lists are processed by the worker threads
     */
    int critical_priority;
    int high_priority;
    int normal_priority;
    int low_priority;

} ServerConfig;


/*  A struct for recording a Gateway and it's last update time */
typedef struct {

    /* network address of wifi link to the Gateway */
    char net_address[NETWORK_ADDR_LENGTH];

    /* The last join request time */
    int last_request_time;

} AddressMap;


typedef struct {

    /* A per list lock */
    pthread_mutex_t list_lock;

    /* A Boolean array in which ith element records whether the ith address map
       is in use. */
    bool in_use[MAX_NUMBER_NODES];

    AddressMap address_map_list[MAX_NUMBER_NODES];

} AddressMapArray;


/* A node of buffer to store received data and/or data to be send */
typedef struct {

    struct List_Entry buffer_entry;

    /* network address of the source or destination */
    char net_address[NETWORK_ADDR_LENGTH];

    /* pointer to where the data is stored. */
    char content[WIFI_MESSAGE_LENGTH];

    int content_size;

} BufferNode;


/* A Head of a list of msg buffers */
typedef struct {

    /* A per list lock */
    pthread_mutex_t list_lock;

    struct List_Entry list_head;

    struct List_Entry priority_list_entry;

    /* nice relative to normal priority (i.e. nice = 0) */
    int priority_nice;

    /* The pointer point to the function to be called to process buffer nodes in
       the list. */
    void (*function)(void *arg);

    /* function's argument */
    void *arg;

} BufferListHead;


/* Global variables */

/* A Server config struct for storing config parameters from the config file */
ServerConfig config;

sbot_api_config api_config;

/* A pointer point to db cursor */
void *Server_db;

/* Struct for storing necessary objects for Wifi connection */
sudp_config udp_config;

/* mempool from which buffer node structure are allocated */
Memory_Pool node_mempool;

/* An array of address maps */
AddressMapArray Gateway_address_map;

/* The head of a list of buffers of data from Gateway */
BufferListHead Gateway_receive_buffer_list_head;

/* The head of a list of buffers of data from LBeacons */
BufferListHead LBeacon_receive_buffer_list_head;

/* The head of a list of buffers for time critical messages */
BufferListHead time_critical_Gateway_receive_buffer_list_head;

/* The head of a list of the return message for the Gateway join requests */
BufferListHead NSI_send_buffer_list_head;

/* The head of a list of buffers for return join request status */
BufferListHead NSI_receive_buffer_list_head;

/* The head of a list of buffers holding health reports to be processed and sent
   to the Server */
BufferListHead BHM_send_buffer_list_head;

/* The head of a list of buffers holding health reports from LBeacons */
BufferListHead BHM_receive_buffer_list_head;

/* The head of a list of buffers for buffer list head in priority order. */
BufferListHead priority_list_head;


/* Flags */

/*
  Initialization of the Server components involves network activates that may
  take time. These flags enable each module to inform the main thread when its
  initialization completes.
 */
bool NSI_initialization_complete;
bool CommUnit_initialization_complete;

bool initialization_failed;

/* Variables for storing the last polling times in second*/
int last_polling_LBeacon_for_HR_time;
int last_polling_object_tracking_time;

/*
  get_config:

     This function reads the specified config file line by line until the
     end of file and copies the data in each line into an element of the
     ServerConfig struct global variable.

  Parameters:

     file_name - the name of the config file that stores the Server data

  Return value:

     ErrorCode - WORK_SUCCESSFULLY: work successfully.
                 E_OPEN_FILE: config file  fail to open.
 */
ErrorCode get_config(ServerConfig *config, char *file_name);


/*
  init_buffer:

     The function fills the attributes of a specified buffer to be called by
     another threads to process the buffer content, including the function, the
     argument of the function and the priority level which the function is to be
     executed.

  Parameters:

     buffer - A pointer of the buffer to be modified.
     buff_id - The index of the buffer for the priority array
     function - A function pointer to be assigned to the buffer
     priority - The priority level of the buffer

  Return value:

     None
 */
void init_buffer(BufferListHead *buffer_list_head, void (*function_p)(void *),
                                int priority_nice);


/*
  sort_priority_list:

     The function arrange entries in the priority list in nonincreasing
     order of Priority_nice.

  Parameters:

     config - The pointer points to the structure which stored config for
              gateway.
     list_head - The pointer of the priority list head.

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
  NSI_routine:

     This function is executed by worker threads when they process the buffer
     nodes in NSI receive buffer list.

  Parameters:

     _buffer_list_head - A pointer points to the buffer to be modified.

  Return value:

     None

 */
void *NSI_routine(void *_buffer_node);

/*
  BHM_routine:

     This function is executed by worker threads when they process the buffer
     nodes in BHM receive buffer list.

  Parameters:

     _buffer_list_head - A pointer points to the buffer to be modified.

  Return value:

     None

 */
void *BHM_routine(void *_buffer_node);


/*
  LBeacon_routine:

     This function is executed by worker threads when they process the buffer
     nodes in LBeacon receive buffer list and send to the server directly.

  Parameters:

     _buffer_list_head - A pointer points to the buffer to be modified.

  Return value:

     None

 */
void *LBeacon_routine(void *_buffer_node);


/*
  Gateway_routine:

     This function is executed by worker threads when they process the buffer
     nodes in Command_msg_buffer_list and broadcast to Gateway.

  Parameters:

     _buffer_list_head - A pointer points to the buffer to be modified.

  Return value:

     None

 */
void *Gateway_routine(void *_buffer_node);


/*
  init_Address_Map:

     This function initialize the head of the AddressMap.

  Parameters:

     address_map - The head of the AddressMap.

  Return value:

     None
 */
void init_Address_Map(AddressMapArray *address_map);


/*
  is_in_Address_Map:

     This function check whether the network address is in Gateway_address_map.

  Parameters:

     address_map - The head of the AddressMap.
     net_address - the network address we decide to compare.

  Return value:

     int: If not find, return -1, else return its array number.
 */
int is_in_Address_Map(AddressMapArray *address_map, char *net_address);


/*
  Gateway_join_request:

     This function is executed when a Gateway sends a command to join the Server
     . When executed, it fills the AddressMap with the inputs and sets the
     network_address if not exceed MAX_NUMBER_NODES.

  Parameters:

     address_map - The head of the AddressMap.
     address - The mac address of the LBeacon IP.

  Return value:

     bool - true  : Join success.
            false : Fail to join

 */
bool Gateway_join_request(AddressMapArray *address_map, char *address
                         );


/*
  Gateway_Broadcast:

     This function is executed when a command needs to be broadcast to Gateways.
     When called, this function sends msg to all Gateways registered in the
     Gateway_address_map.

  Parameters:
     address_map - The head of the AddressMap.
     msg - The pointer points to the msg to be send to beacons.
     size - The size of the msg.

  Return value:

     None

 */
void Gateway_Broadcast(AddressMapArray *address_map, char *msg, int size);


/*
  Wifi_init:

     This function initializes the Wifi's necessory object.

  Parameters:

     IPaddress - The IP address of the server.

  Return value:

      ErrorCode - The error code for the corresponding error or successful

 */
ErrorCode Wifi_init(char *IPaddress);


/*
  Wifi_free:

     When called, this function frees the queue of the Wi-Fi pkts and sockets.

  Parameters:

     None

  Return value:

     None

 */
void Wifi_free();


/*
  process_wifi_send:

     This function sends the msg in the buffer list to the server via Wi-Fi.

  Parameters:

     _buffer_list_head - A pointer points to the buffer list head.

  Return value:

     None
 */
void *process_wifi_send(void *_buffer_node);


/*
  process_wifi_receive:

     This function listens for messages or command received from the server or
     beacons. After getting the message, push the data in the message into the
     buffer.

  Parameters:

     None

  Return value:

     None
 */
void *process_wifi_receive();

#endif
