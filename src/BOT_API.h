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

     1.0, 20190326

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

    /* The port number which the module use for receiving message from the api
       module */
    int module_dest_port;

    /* The port number which is use for the api module to receive message from
       the module such as Geo-Fencing. */
    int api_recv_port;

    /* worker threads for processing scheduled tasks from modules by assign a
       worker thread. */
    Threadpool schedule_workers;

    /* Number of schedule_worker allow to use */
    int number_schedule_workers;

    /* The schedule stored in the buffer list will be executed when data update
     */
    pschedule_list_head event_schedule_list;

    /* The schedule stored in the buffer list will be executed within 1 hour */
    pschedule_list_head short_term_schedule_list;

    /* The schedule stored in the buffer list will be executed within 1 day but
       more than 1 hour */
    pschedule_list_head medium_term_schedule_list;

    /* The schedule stored in the buffer list will be executed more than 1 day
     */
    pschedule_list_head long_term_schedule_list;

} sbot_api_config;

typedef sbot_api_config *pbot_api_config;


typedef struct {

    /* The interval in seconds that the worker thread start to scan the list */
    int scan_periods;

    /* The mutex use for locking the list when update or scan the list */
    pthread_mutex_t mutex;

    /* The mempool use for the node in this schedule list */
    Memory_Pool schedule_node_mempool;

    /* The pointer point to the function to be called to schedule nodes in
       the list. */
    void (*function)(void *arg);

    /* function's argument */
    void *arg;

    /* The head of this schedule list */
    List_Entry list_head;

} sschedule_list_head;

typedef sschedule_list_head *pschedule_list_head;


typedef struct {

    /* The id in the BOT database */
    int id;

    /* The type of the schedule */
    int type;

    /* The time to process the schedule.
       When the type is "once", the processing time is a timestamp.
       When the type is "period", the processing time is a period of seconds.
     */
    int period_of_process_time;

    /* The id of data type the schedule is required. */
    int data_type;

    /* If the type of the schedule is "period", it will use to count next
       trigger time. */
    int remain_time;

    char *ip_address[NETWORK_ADDR_LENGTH];

    List_Entry list_node;

} sschedule_list_node;

typedef sschedule_list_node *pschedule_list_node;


/*
  bot_api_initial:

     This function initialize API and sockets in BOT system.
     * Sockets including UDP sender and receiver.

  Parameters:

     api_config - The pointer points to the api_config.
     number_worker_thread - The number of worker thread will be use for
                            processing schedules
     module_dest_port - The port number of the module to which the api is sent.
     api_recv_port - The port number of the api for receiving data from modules.

  Return value:

     ErrorCode

 */
ErrorCode bot_api_initial(pbot_api_config api_config, int number_worker_thread,
                          int module_dest_port, int api_recv_port );


/*
  bot_api_free:

     The function release sockets for sending and receiving and schedule lists
     for scheduling.

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

     _api_config - The pointer points to the api config.

  Return value:

     None

 */
void *bot_api_schedule_routine(void *_api_config);


/*
  process_schedule_routine:

     This function is executed by worker threads when processing schedule lists
     which the type of the schedule list is "period".

  Parameters:

     _schedule_list - The pointer points to the schedule list head.

  Return value:

     None

 */
void *process_schedule_routine(void *_schedule_list);


/*
  process_api_send:

     This function is executed by worker threads when sending the msg to
     modules.

  Parameters:

     _schedule_node - The pointer points to the node in the schedule list head.

  Return value:

     None

 */
void *process_api_send(void *_schedule_node);


/*
  process_api_recv:

     This function is executed by worker threads when reveiving msgs from
     modules.

  Parameters:

     _api_config - The pointer points to the api config.

  Return value:

     None

 */
void *process_api_recv(void *_api_config);


/*
  init_schedule_list:

     This function initialize the schedule list.

  Parameters:

     schedule_list - The pointer points to the schedule list head.
     function - The pointer points to the function to be called to process
                schedule nodes in the schedule list.

  Return value:

     ErrorCode

 */
 ErrorCode init_schedule_list(pschedule_list_head schedule_list,
                              void (*function_p)(void *) );

/*
  free_schedule_list:

     This function free the schedule list.

  Parameters:

     schedule_list - The pointer points to the schedule list head.

  Return value:

     ErrorCode

 */
ErrorCode free_schedule_list(pschedule_list_head schedule_list);


#endif
