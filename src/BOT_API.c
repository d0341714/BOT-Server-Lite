/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     BOT_API.c

  File Description:

     This header file contains programs to send request, subscribe, publish and
	 process data from modules.

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

#include "BOT_API.h"


ErrorCode bot_api_initial(pbot_api_config api_config, int number_worker_thread,
                                                      int module_dest_port,
                                                      int api_recv_port){

    api_config -> module_dest_port = module_dest_port;

    api_config -> api_recv_port = api_recv_port;

    api_config -> number_schedule_workers = number_worker_thread;

    api_config -> schedule_workers = thpool_init(number_worker_thread);

    init_schedule_list(api_config -> event_schedule_list,
                       (void *)bot_api_schedule_routine);
    init_schedule_list(api_config -> short_term_schedule_list,
                       (void *)bot_api_schedule_routine);
    init_schedule_list(api_config -> medium_term_schedule_list,
                       (void *)bot_api_schedule_routine);
    init_schedule_list(api_config -> long_term_schedule_list,
                       (void *)bot_api_schedule_routine);

    if (udp_initial(&api_config -> udp_config, module_dest_port, api_recv_port)
        == WORK_SUCCESSFULLY){
        return E_WIFI_INIT_FAIL;
    }

    return WORK_SUCCESSFULLY;
}


ErrorCode bot_api_free(pbot_api_config api_config){

    udp_release( &api_config -> udp_config);

    init_schedule_list(api_config -> event_schedule_list,
                       (void *)bot_api_schedule_routine);
    init_schedule_list(api_config -> short_term_schedule_list,
                       (void *)bot_api_schedule_routine);
    init_schedule_list(api_config -> medium_term_schedule_list,
                       (void *)bot_api_schedule_routine);
    init_schedule_list(api_config -> long_term_schedule_list,
                       (void *)bot_api_schedule_routine);

}


void * bot_api_schedule_routine(void *_schedule_node){

}


void *process_schedule_routine(){




}


ErrorCode init_schedule_list(pschedule_list_head schedule_list,
                             void (*function_p)(void *) ){

    pthread_mutex_init( &schedule_list -> mutex);

    mp_init( &(schedule_list -> schedule_node_mempool),
            sizeof(sschedule_list_node),
            SLOTS_IN_MEM_POOL);

    init_entry( &(schedule_list -> list_head));

    schedule_list -> function = function_p;

    schedule_list -> arg = (void *)schedule_list;

}


ErrorCode free_schedule_list(pschedule_list_head schedule_list ){

    pthread_mutex_destroy(&schedule_list -> mutex);

    mp_destroy( &(schedule_list -> list_head));

}
