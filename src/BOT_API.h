/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     BOT_API.h

  File Description:

     This header file contains the function declarations and variables used in
     the BOT_API.c file.

  Version:

     1.0, 20190403

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

 */

#ifndef BOT_API_H
#define BOT_API_H


#include "BeDIS.h"


typedef struct {

    /* UDP for receiving or sending message from and to the modules */
    sudp_config udp_config;

    void *db;

    /* The port number which the module use for receiving message from the api
       module */
    int module_dest_port;

    /* The port number which is use for the api module to receive message from
       the module such as Geo-Fencing. */
    int api_recv_port;

    /* Worker threads for processing scheduled tasks from modules by assign a
       worker thread. */
    Threadpool schedule_workers;

    Memory_Pool pkt_content_mempool;

    /* Number of schedule_worker allow to use */
    int number_schedule_workers;

    bool is_running;

    pthread_t process_api_recv_thread;

} sbot_api_config;

typedef sbot_api_config *pbot_api_config;


typedef struct {

    char ip_address[NETWORK_ADDR_LENGTH];

    char content[WIFI_MESSAGE_LENGTH];

    int content_size;

    pbot_api_config api_config;

} spkt_content;

typedef spkt_content *ppkt_content;


/*
  bot_api_initial:

     This function initialize API and sockets in BOT system.
     * Sockets including UDP sender and receiver.

  Parameters:

     api_config - The pointer points to the api_config.
     db - a pointer pointing to the connection to the database backend server.
     number_worker_thread - The number of worker thread will be use for
                            processing schedules
     module_dest_port - The port number of the module to which the api is sent.
     api_recv_port - The port number of the api for receiving data from modules.

  Return value:

     ErrorCode

 */
ErrorCode bot_api_initial(pbot_api_config api_config, void *db,
                          int number_worker_thread, int module_dest_port,
                          int api_recv_port );


/*
  bot_api_free:

     The function release sockets for sending and receiving.

  Parameters:

     api_config - The pointer points to the api_config.

  Return value:

     ErrorCode

 */
ErrorCode bot_api_free(pbot_api_config api_config);


/*
  bot_api_schedule_routine:

     This function is executed by worker threads when processing data
     subscription, public and update and processing the type of the schedule
     lists is "update" .

  Parameters:

     _pkt_content - The pointer points to the node.

  Return value:

     None

 */
static void *bot_api_schedule_routine(void *_pkt_content);


/*
  process_schedule_routine:

     This function is executed by worker threads when processing schedule lists
     which the type of the schedule list is "period".

  Parameters:

     _pkt_content - The pointer points to the node.

  Return value:

     None

 */
static void *process_schedule_routine(void *_pkt_content);


/*
  process_api_recv:

     This function is executed by worker threads when reveiving msgs from
     modules.

  Parameters:

     _api_config - The pointer points to the api config.

  Return value:

     None

 */
static void *process_api_recv(void *_api_config);


#endif
