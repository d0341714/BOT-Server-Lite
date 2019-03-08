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

/* When debugging is needed */
//#define debugging

/* Gateway config file location and defining of config file. */

/* File path of the config file of the Gateway */
#define CONFIG_FILE_NAME "./config/server.conf"

/* File path of the config file of the zlog */
#define ZLOG_CONFIG_FILE_NAME "/home/pi/BOT-Server-Lite/config/zlog.conf"

/* The category defined of log file used for health report */
#define LOG_CATEGORY_HEALTH_REPORT "Health_Report"

#define MAXIMUM_DATABASE_INFO 1024

#ifdef debugging
/* The category defined for the printf during debugging */
#define LOG_CATEGORY_DEBUG "LBeacon_Debug"

#endif

/* Maximum number of nodes (LBeacons) per star network rooted at a gateway */
#define MAX_NUMBER_NODES 32

#define TEST_MALLOC_MAX_NUMBER_TIMES 5

/*
  Maximum length of time in seconds low priority message lists are starved
  of attention. */
#define MAX_STARVATION_TIME 600


/* The configuration file structure */
typedef struct {

    /* The IP address of server for WiFi netwok connection. */
    char server_ip[NETWORK_ADDR_LENGTH];

    /* The IP address of database for server to connect. */
    char db_ip[NETWORK_ADDR_LENGTH];
    
    /* The number of LBeacon nodes in the star network of this gateway */
    int allowed_number_nodes;

    /* The time interval in seconds for gateway sending request for health
       report data to LBeacon */
    int period_between_RFHR;

    /* The time interval in seconds for gateway sending request for tracked
       object data to LBeacon */
    int period_between_RFTOD;

    /*The number of worker threads used by the communication unit for sending
      and receiving packets to and from LBeacons and the sever.*/
    int number_worker_threads;

    /* A port which beacons and server are listening on and for gateway to send
       to. */
    int send_port;

    /* A port which gateway is listening for beacons and server to send to */
    int recv_port;
    
    /* A port with database is lisening for server to send to */
    int database_port;

    char database_name[MAXIMUM_DATABASE_INFO];

    char database_account[MAXIMUM_DATABASE_INFO];

    char database_password[MAXIMUM_DATABASE_INFO];


    /* each priority levels */
    int critical_priority;
    int high_priority;
    int normal_priority;
    int low_priority;

} ServerConfig;


/*  A struct linking network address assigned to a LBeacon to its UUID,
    coordinates, and location description. */
typedef struct {

    /* network address of wifi link to the LBeacon*/
    char net_address[NETWORK_ADDR_LENGTH];

    /* The last request join time */
    int last_request_time;

} AddressMap;


typedef struct {

    /* A per list lock */
    pthread_mutex_t list_lock;

    /* A bit for recording whether the address map is in use. */
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


/* A Head of a list of msg buffer */
typedef struct {

    /* A per list lock */
    pthread_mutex_t list_lock;

    struct List_Entry priority_list_entry;

    struct List_Entry list_head;

    /* nice relative to normal priority (i.e. nice = 0) */
    int priority_nice;

    /* Point to the function to be called to process buffer node in this list.
     */
    void (*function)(void *arg);

    /* function's argument */
    void *arg;

} BufferListHead;


/* Global variables */

/* A Gateway config struct stored config from the config file */
ServerConfig config;

/* A pointer point to db cursor */
void *Server_db;

/* Struct for storing necessary objects for Wifi connection */
sudp_config udp_config;

/* mempool of node for Gateway */
Memory_Pool node_mempool;

/* A list of address maps */
AddressMapArray Gateway_address_map;

/* For data from Gateway to be send to server */
BufferListHead Gateway_receive_buffer_list_head;

/* For data from LBeacon to be send to server */
BufferListHead LBeacon_receive_buffer_list_head;

/* For time critical messages list head */
BufferListHead time_critical_Gateway_receive_buffer_list_head;

/* For beacon join request */
BufferListHead NSI_send_buffer_list_head;

/* For return join request status */
BufferListHead NSI_receive_buffer_list_head;

/* For processed health reports to be send to server */
BufferListHead BHM_send_buffer_list_head;

/* For health reports from LBeacons */
BufferListHead BHM_receive_buffer_list_head;

/* Head of a list of buffer_list_head in the priority order. */
BufferListHead priority_list_head;


/* Flags */

/*
  Initialization of gateway components involves network activates that may take
  time. These flags enable each module to inform the main thread when its
  initialization completes.
 */
bool NSI_initialization_complete;
bool CommUnit_initialization_complete;
bool BHM_initialization_complete;

bool initialization_failed;

/* Store last polling time in second*/
int last_polling_LBeacon_for_HR_time;
int last_polling_object_tracking_time;

/*
  get_config:

     This function reads the specified config file line by line until the
     end of file and copies the data in the lines into the ServerConfig
     struct global variable.

  Parameters:

     file_name - the name of the config file that stores gateway data

  Return value:

     config - ServerConfig struct
 */
ErrorCode get_config(ServerConfig *config, char *file_name);


/*
  init_buffer:

     The function fills the attributes of buffer storing the packets.
     Including assigning the function to the corresponding buffer list and its
     arguments and its priority level.

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
  sort_priority:

     The function arrange entries in the priority_list_head in nonincreasing
     order of Priority_nice.

  Parameters:

     list_head - The pointer of priority_list_head.

  Return value:

     None
 */
void *sort_priority(BufferListHead *list_head);


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
     nodes in NSI_receive_buffer_list.

  Parameters:

     _buffer_list_head - A pointer of the buffer to be modified.

  Return value:

     None

 */
void *NSI_routine(void *_buffer_list_head);

/*
  BHM_routine:

     This function is executed by worker threads when they process the buffer
     nodes in BHM_receive_buffer_list.

  Parameters:

     _buffer_list_head - A pointer of the buffer to be modified.

  Return value:

     None

 */
void *BHM_routine(void *_buffer_list_head);


/*
  LBeacon_routine:

     This function is executed by worker threads when they process the buffer
     nodes in LBeacon_receive_buffer_list and send to the server directly.

  Parameters:

     _buffer_list_head - A pointer of the buffer to be modified.

  Return value:

     None

 */
void *LBeacon_routine(void *_buffer_list_head);


/*
  Gateway_routine:

     This function is executed by worker threads when they process the buffer
     nodes in Command_msg_buffer_list and broadcast to Gateway.

  Parameters:

     _buffer_list_head - A pointer of the buffer to be modified.

  Return value:

     None

 */
void *Gateway_routine(void *_buffer_list_head);


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

     bool: If return true means in the address map, else false.
 */
bool is_in_Address_Map(AddressMapArray *address_map, char *net_address);


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
     msg - The pointer to the msg to be send to beacons.
     size - The size of the msg.

  Return value:

     None

 */
void Gateway_Broadcast(AddressMapArray *address_map, char *msg, int size);


/*
  Wifi_init:

     This function initializes the Wifi's necessory object.

  Parameters:

     IPaddress - The address of the server.

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

     _buffer_list_head - A pointer to the buffer list head.

  Return value:

     None
 */
void *process_wifi_send(void *_buffer_list_head);


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
