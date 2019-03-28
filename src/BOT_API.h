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

    /* worker threads for processing scheduled tasks from modules by assign a
       worker thread. */
    Threadpool schedule_workers;

    /* Number of schedule_worker allow to use */
    int number_schedule_workers;

    Memory_Pool schedule_node_mempool;

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

    int scan_periods;

    pthread_mutex_t mutex;

    /* The pointer point to the function to be called to schedule nodes in
       the list. */
    void (*function)(void *arg);

    /* function's argument */
    void *arg;

    List_Entry list_head;

} sschedule_list_head;

typedef sschedule_list_head *pschedule_list_head;


typedef struct {

    int id;

    int type;

    List_Entry list_node;

} sschedule_list_node;

typedef sschedule_list_node *pschedule_list_node;


/*
  bot_api_initial:

     This function initialize API and sockets in BOT system.
     sockets including UDP sender and receiver.

  Parameters:

     pbot_api_config - The pointer points to the api_config.
     module_dest_port - The port number of the module to which the api is sent.
     api_recv_port - The port number of the api for receiving data from modules.

  Return value:

     ErrorCode

 */
ErrorCode bot_api_initial(pbot_api_config api_config, int module_dest_port,
                          int api_recv_port );


/*
  bot_api_recv_routine:


  Parameters:


  Return value:

 */
ErrorCode bot_api_recv_routine();


/*
  bot_api_recv_routine:


  Parameters:


  Return value:

 */
ErrorCode bot_api_recv_routine();


/*
  bot_api_schedule_routine:


  Parameters:


  Return value:

 */
ErrorCode bot_api_schedule_routine();


/*
  bot_api_release:


  Parameters:


  Return value:

 */
ErrorCode bot_api_release();


#endif
