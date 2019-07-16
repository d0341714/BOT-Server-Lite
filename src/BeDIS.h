/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     BeDIS.h

  File Description:

     This file contains the definitions and declarations of constants,
     structures, and functions used in Server, Gateway and LBeacon.

  Version:

     2.0, 20190617

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

     Gary Xiao     , garyh0205@hotmail.com
     Joey Zhou     , joeyzhou5566@gmail.com
     Holly Wang    , hollywang@iis.sinica.edu.tw
     Jake Lee      , jakelee@iis.sinica.edu.tw
     Chun Yu Lai   , chunyu1202@gmail.com

 */

#ifndef BEDIS_H
#define BEDIS_H


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <winsock2.h>
#pragma comment(lib,"WS2_32.lib")
#include <WS2tcpip.h>
#include <signal.h>
#include <time.h>
#include <windows.h>
#include "Mempool.h"
#include "UDP_API.h"
#include "LinkedList.h"
#include "thpool.h"
#include "zlog.h"
#include <windows.h>



/* Parameter that marks the start of the config file */
#define DELIMITER "="

/* Parameter that marks the start of the fraction part of float number */
#define FRACTION_DOT "."

/* Parameter that marks the separator of differnt records communicated with
   SQL wrapper API */
#define DELIMITER_SEMICOLON ";"

/* Parameter that marks the separate of different records */
#define DELIMITER_COMMA ","

/* Maximum number of characters in each line of config file */
#define CONFIG_BUFFER_SIZE 64

/* Number of times to retry open file, because file openning operation may have
   transient failure. */
#define FILE_OPEN_RETRIES 5

/* Number of times to retry getting a dongle, because this operation may have
   transient failure. */
#define DONGLE_GET_RETRIES 5

/* Number of times to retry opening socket, because socket openning operation
   may have transient failure. */
#define SOCKET_OPEN_RETRIES 5

/* The number of slots in the memory pool */
#define SLOTS_IN_MEM_POOL 1024

/* Length of the IP address in byte */
#define NETWORK_ADDR_LENGTH 16

/* Length of the IP address in Hex */
#define NETWORK_ADDR_LENGTH_HEX 8

/* The size of message to be sent over WiFi in bytes */
#define WIFI_MESSAGE_LENGTH 4096

/* Maximum length of the message in bytes allow to set to WIFI_MESSAGE_LENGTH */
#define MAXIMUM_WIFI_MESSAGE_LENGTH 65507

/* Minimum length of the message in bytes
   (One byte for data type and one byte for a space) 
 */
#define MINIMUM_WIFI_MESSAGE_LENGTH 2

/* Number of characters in a Bluetooth MAC address */
#define LENGTH_OF_MAC_ADDRESS 18

/* Maximum length of message to communicate with SQL wrapper API in bytes */
#define SQL_TEMP_BUFFER_LENGTH 4096

/* The size of array to store Wi-Fi SSID */
#define WIFI_SSID_LENGTH 10

/* The size of array to store Wi-Fi Password */
#define WIFI_PASS_LENGTH 10

/* Length of the LBeacon's UUID in number of characters */
#define UUID_LENGTH 32

/* Length of coordinates in number of bits */
#define COORDINATE_LENGTH 64

/* The port on which to listen for incoming data */
#define UDP_LISTEN_PORT 8888

/* Number of bytes in the string format of epoch time */
#define LENGTH_OF_EPOCH_TIME 11

/* Time interval in seconds for busy-wait checking in threads */
#define INTERVAL_FOR_BUSY_WAITING_CHECK_IN_SEC 3

/* Timeout interval in ms */
#define BUSY_WAITING_TIME 5

/* Maximum length for each array of database information */
#define MAXIMUM_DATABASE_INFO 1024

/* Maximum number of nodes per star network */
#define MAX_NUMBER_NODES 16

/*
  Maximum length of time in seconds low priority message lists are starved
  of attention. */
#define MAX_STARVATION_TIME 600


typedef enum _ErrorCode{

    WORK_SUCCESSFULLY = 0,
    E_MALLOC = 1,
    E_OPEN_FILE = 2,
    E_OPEN_DEVICE = 3,
    E_OPEN_SOCKET = 4,
    E_SEND_OBEXFTP_CLIENT = 5,
    E_SEND_CONNECT_DEVICE = 6,
    E_SEND_PUSH_FILE = 7,
    E_SEND_DISCONNECT_CLIENT = 8,
    E_SCAN_SET_EVENT_MASK = 9,
    E_SCAN_SET_ENABLE = 10,
    E_SCAN_SET_HCI_FILTER = 11,
    E_SCAN_SET_INQUIRY_MODE = 12,
    E_SCAN_START_INQUIRY = 13,
    E_SEND_REQUEST_TIMEOUT = 14,
    E_ADVERTISE_STATUS = 15,
    E_ADVERTISE_MODE = 16,
    E_SET_BLE_PARAMETER = 17,
    E_BLE_ENABLE = 18,
    E_GET_BLE_SOCKET = 19,
    E_START_THREAD = 20,
    E_INIT_THREAD_POOL = 21,
    E_INIT_ZIGBEE = 22,
    E_LOG_INIT = 23,
    E_LOG_GET_CATEGORY = 24,
    E_EMPTY_FILE = 25,
    E_INPUT_PARAMETER = 26,
    E_ADD_WORK_THREAD = 27,
    E_INITIALIZATION_FAIL = 28,
    E_WIFI_INIT_FAIL = 29,
    E_START_COMMUNICAT_ROUTINE_THREAD = 30,
    E_START_BHM_ROUTINE_THREAD = 31,
    E_START_TRACKING_THREAD = 32,
    E_REG_SIG_HANDLER = 33,
    E_JOIN_THREAD = 34,
    E_BUFFER_SIZE = 35,
    E_PREPARE_RESPONSE_BASIC_INFO = 36,
    E_ADD_PACKET_TO_QUEUE = 37,
    E_SQL_OPEN_DATABASE = 38,
    E_SQL_PARSE = 39,
    E_SQL_RESULT_EXCEED = 40,
    E_SQL_EXECUTE = 41,
    E_API_INITIALLZATION =42,
    E_API_FREE = 43,
    E_MODULE_INITIALIZATION = 44,

    MAX_ERROR_CODE = 45

} ErrorCode;


/* Type of health_status to be queried. */
typedef enum _HealthStatus {

    S_NORMAL_STATUS = 0,
    E_ERROR_STATUS = 1,
    MAX_STATUS = 2

} HealthStatus;


typedef enum pkt_types {
    /* Unknown type of pkt type */
    undefined = 0,

    /* For Join Request */

    /* Request join from LBeacon */
    request_to_join = 1,
    /* When Gateway accept LBeacon join request */
    join_request_ack = 2,
    /* When Gateway deny Beacon join request */
    join_request_deny = 3,

    /* For LBeacon send type */

    /* A pkt containing tracked object data */
    tracked_object_data = 4,
    /* A pkt containing health report */
    health_report = 5,
    /* A pkt for LBeacon */
    data_for_LBeacon = 6,

    /* GeoFence */

    /* A pkt containing GeoFence data */
    GeoFence_data = 7,
    /* A pkt containing GeoFence alert data */
    GeoFence_alert_data = 8

} PktType;


typedef enum pkt_direction {

    /* pkt from gateway */
    from_gateway = 10,
    /* pkt from server */
    from_server = 8,
    /* pkt from beacon */
    from_beacon = 0,
    /* pkt from modules */
    from_modules = 12

} PktDirection;


/* Type of device to be tracked. */
typedef enum DeviceType {

    BR_EDR = 0,
    BLE = 1,
    max_type = 2

} DeviceType;


/* A node of buffer to store received data and/or data to be send */
typedef struct {

    struct List_Entry buffer_entry;

    unsigned int pkt_direction;

    unsigned int pkt_type;

    /* The network address of the packet received or the packet to be sent */
    char net_address[NETWORK_ADDR_LENGTH];

    /* The port of the packet received or the packet to be sent */
    unsigned int port;

    /* The pointer points to the content. */
    char content[WIFI_MESSAGE_LENGTH];

    /* The size of the content */
    int content_size;

} BufferNode;


/* A Head of a list of msg buffers */
typedef struct {

    /* A per list lock */
    pthread_mutex_t list_lock;

    struct List_Entry list_head;

    struct List_Entry priority_list_entry;

    /* The nice is a value relative to the normal priority (i.e. nice = 0) */
    int priority_nice;

    /* The pointer point to the function to be called to process buffer nodes in
       the list. */
    void (*function)(void *arg);

    /* The argument of the function */
    void *arg;

} BufferListHead;


/*  A struct for recording the network address and it's last update time */
typedef struct {

    /* The network address of wifi link to the Gateway */
    char net_address[NETWORK_ADDR_LENGTH];

    /* The last join request time */
    int last_request_time;

} AddressMap;


typedef struct {

    /* A per array lock for the AddressMapArray when reading and update data */
    pthread_mutex_t list_lock;

    /* A Boolean array in which ith element records whether the ith address map
       is in use. */
    bool in_use[MAX_NUMBER_NODES];

    AddressMap address_map_list[MAX_NUMBER_NODES];

} AddressMapArray;


typedef struct coordinates{

    char X_coordinates[COORDINATE_LENGTH];
    char Y_coordinates[COORDINATE_LENGTH];
    char Z_coordinates[COORDINATE_LENGTH];

} Coordinates;


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

} ServerConfig;

/* The Server config struct for storing config parameters from the config file 
 */
ServerConfig serverconfig;

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


/* A global flag that is initially set to true by the main thread. It is set
   to false by any thread when the thread encounters a fatal error,
   indicating that it is about to exit. In addition, if user presses Ctrl+C,
   the ready_to_work flag will be set as false to stop all threads. */
bool ready_to_work;

/* The pointer to the category of the log file */
zlog_category_t *category_health_report, *category_debug;

/* FUNCTIONS */

/*
  twoc:

     @todo

  Parameters:

     in - @todo
     t  -  @todo

  Return value:

     data - @todo
 */
unsigned int twoc(int in, int t);


/*
  init_buffer:

     The function fills the attributes of a specified buffer content will 
     be process by another thread, including the function need to process 
     the content, the argument of the function and the priority level at 
     which the function is to be executed.

  Parameters:

     buffer - A pointer points to the buffer to be processed.
     buff_id - The index of the buffer for the priority array.
     function - The pointer to the function as way the attributes of the buffer.
     priority - The priority nice of the thread when processing the buffer.

  Return value:

     None
 */
void init_buffer(BufferListHead *buffer_list_head, void (*function_p)(void *),
                 int priority_nice);


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

     This function check whether the network address is in the AddressMap.

  Parameters:

     address_map - A pointer to the head of the AddressMap.
     net_address - The pointer to the network address to compare.

  Return value:

     int: If not find, return -1, else return its array number.
 */
int is_in_Address_Map(AddressMapArray *address_map, char *net_address);


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
  udp_sendpkt

     This function is called to send a packet to the destination via UDP 
     connection.

  Parameter:

     udp_config  : A pointer to the structure contains all variables 
                   for the UDP connection.
     buffer_node : A pointer to the buffer node.

  Return Value:

     int : If return 0, everything work successfully.
           If not 0   , something wrong.
 */
int udp_sendpkt(pudp_config udp_config, BufferNode *buffer_node);


/*
  trim_string_tail:

     This function trims whitespace, newline and carry-return at the end of
     the string when reading config messages.

  Parameters:

     message - A pointer to the character array containing the input
               string.

  Return value:

     None
 */
void trim_string_tail(char *message);


/*
  ctrlc_handler:

     When the user presses CTRL-C, the function sets the global variable
     ready_to_work to false, and throw a signal to stop running the program.

  Parameters:

     stop - A interger signal triggered by ctrl-c.

  Return value:

     None

 */
void ctrlc_handler(int stop);

/*
  strncasecmp:

     This function compares two input strings speicified by input parameters.

  Parameters:

     str_a - the first string to be compared
     str_b - the second string to be comapred
     len - number of characters in the strings to be compared

  Return value:
     0: if the two strings exactly match
 */

int strncasecmp(char const *str_a, char const *str_b, size_t len);


/*
  startThread:

     This function initializes the specified thread. Threads initialized by
     this function will be create in detached mode.

  Parameters:

     thread        - A pointer of the thread.
     start_routine - Routine to be executed by the thread.
     arg           - The argument for the start_routine.

  Return value:

     Error_code: The error code for the corresponding error
 */
ErrorCode startThread(pthread_t *thread, void *( *start_routine)(void *),
                      void *arg);


/*
  strtok_save:
     
     This function breaks a specified string into a series of tokens using the 
     delimiter delim.
     
     Windows uses strtok_s()

  Parameters:

     str     - A pointer to the string to be modified and broken into smaller    
               strings (tokens).
     delim   - The C string containing the delimiters.
     saveptr - A pointer to the next char of the searched character.

  Return value:

      Return a pointer to the next token, or NULL if there are no more tokens.

 */
char *strtok_save(char *str, char *delim, char **saveptr);


/*
  get_system_time:

     This helper function fetches the current time according to the system
     clock in terms of the number of seconds since January 1, 1970.

  Parameters:

     None

  Return value:

     system_time - system time in seconds
*/
int get_system_time();


#endif