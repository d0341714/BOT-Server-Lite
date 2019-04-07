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

     This file contains programs to send request, subscribe, publish and
	 process data from and to modules.

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

#include "BOT_API.h"


ErrorCode bot_api_initial(pbot_api_config api_config , void *db,
                          int number_worker_thread, int module_dest_port,
                          int api_recv_port){

    api_config -> is_running = true;

    api_config -> module_dest_port = module_dest_port;

    api_config -> api_recv_port = api_recv_port;

    api_config -> number_schedule_workers = number_worker_thread;

    api_config -> schedule_workers = thpool_init(number_worker_thread);

    if (db != NULL)
        api_config -> db = db;
    else
        return E_SQL_OPEN_DATABASE;

    if(mp_init( &(api_config -> pkt_content_mempool), sizeof(spkt_content),
               SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if (udp_initial(&api_config -> udp_config, module_dest_port, api_recv_port)
        != WORK_SUCCESSFULLY)
        return E_WIFI_INIT_FAIL;

    if (startThread( &api_config -> process_api_recv_thread,
        (void *)process_api_recv, api_config) != WORK_SUCCESSFULLY){
        return E_START_THREAD;
    }

    return WORK_SUCCESSFULLY;
}


ErrorCode bot_api_free(pbot_api_config api_config){

    api_config -> is_running = false;

    Sleep(WAITING_TIME);

    udp_release( &api_config -> udp_config);

    thpool_destroy(&api_config -> schedule_workers);

    mp_destroy(&api_config -> pkt_content_mempool);

}


static void *bot_api_schedule_routine(void *_pkt_content){

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    int data_type;

    /* read the pkt type from lower lower 4 bits. */
    data_type = pkt_content -> content[0] & 0x0f;

    switch(data_type){
        case add_data_type:
        case del_data_type:

            SQL_update_api_data_type(pkt_content -> api_config -> db,
                                     pkt_content -> ip_address,
                                     &(pkt_content -> content[1]),
                                     pkt_content -> content_size - 1);

            break;

        case add_subscriber:
        case del_subscriber:

            SQL_update_api_subscription(pkt_content -> api_config -> db,
                                        pkt_content -> ip_address,
                                        &(pkt_content -> content[1]),
                                        pkt_content -> content_size - 1);

            break;

    }

    mp_free( &pkt_content -> api_config -> pkt_content_mempool, _pkt_content);

}


static void *process_schedule_routine(void *_pkt_content){

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    unsigned long int data_type_id;
    char data_content[WIFI_MESSAGE_LENGTH];
    char *current_data_pointer = NULL;
    char *return_data;

    sscanf(&(pkt_content -> content[1]), "%lu;%s", data_type_id,
                                                   data_content);

    return_data = SQL_get_subscriber(pkt_content -> api_config -> db,
                                     data_type_id);

    if(return_data != NULL && return_data[0] != DELIMITER_SEMICOLON){
        printf ("Splitting return_data \"%s\":\n",return_data);

        current_data_pointer = strtok(return_data, DELIMITER_SEMICOLON);

        while(current_data_pointer != NULL){
            printf ("Current Process IP: %s\n",current_data_pointer);

            udp_addpkt(pkt_content -> api_config,
                       current_data_pointer,
                       &pkt_content -> content[1],
                       pkt_content -> content_size -1);

            current_data_pointer = strtok (NULL, DELIMITER_SEMICOLON);
        }
    }

    mp_free(& pkt_content -> api_config -> pkt_content_mempool,
            _pkt_content);

}


static void *process_api_recv(void *_api_config){

    int return_value;

    int pkt_direction;

    int pkt_type;

    sPkt temppkt;

    char *tmp_addr;

    ppkt_content pkt_content;

    pbot_api_config api_config = (pbot_api_config)_api_config;

    while(api_config -> is_running == true){

        temppkt = udp_getrecv( &api_config -> udp_config);

        if(temppkt.type == UDP){

            tmp_addr = udp_hex_to_address(temppkt.address);

            pkt_content = mp_alloc(&(api_config -> pkt_content_mempool));

            memset(pkt_content, 0, strlen(pkt_content) * sizeof(char));

            memcpy(pkt_content -> ip_address, tmp_addr, strlen(tmp_addr) *
                   sizeof(char));

            memcpy(pkt_content -> content, temppkt.content,
                   temppkt.content_size);

            pkt_content -> content_size = temppkt.content_size;

            pkt_content -> api_config = api_config;

            while(api_config -> schedule_workers -> num_threads_working ==
                  api_config -> schedule_workers -> num_threads_alive){
                Sleep(WAITING_TIME);
            }

            /* read the pkt direction from higher 4 bits. */
            pkt_direction = (temppkt.content[0] >> 4) & 0x0f;
            /* read the pkt type from lower lower 4 bits. */
            pkt_type = temppkt.content[0] & 0x0f;

            switch (pkt_direction) {

                case from_modules:

                    switch (pkt_type) {

                        case add_data_type:
                        case del_data_type:
                        case add_subscriber:
                        case del_subscriber:

                            return_value = thpool_add_work(
                                            api_config -> schedule_workers,
                                            bot_api_schedule_routine,
                                            pkt_content,
                                            0);
                            break;

                        case update_data:

                            return_value = thpool_add_work(
                                            api_config -> schedule_workers,
                                            process_schedule_routine,
                                            pkt_content,
                                            0);
                            break;

                    }
                    break;

            }

            free(tmp_addr);
        }
    }
}
