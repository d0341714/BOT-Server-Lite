/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Geo-Fencing.c

  File Description:

     This file contains programs for detecting whether objects we tracked and
     restricted in the fence triggers Geo-Fence.

  Version:

     1.0, 20190415

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

#include "Geo-Fencing.h"


void *geo_fence_routine(void *_geo_fence_config){

    pgeo_fence_config geo_fence_config = (pgeo_fence_config)_geo_fence_config;

    pudp_config udp_config = &geo_fence_config -> udp_config;

    int last_subscribe_geo_fence_data_time,
        last_subscribe_tracked_object_data_time;

    char content[WIFI_MESSAGE_LENGTH];

    geo_fence_config -> is_running = true;

    geo_fence_config -> worker_thread = thpool_init(
                                   geo_fence_config -> number_schedule_workers);

    geo_fence_config -> geo_fence_data_subscribe_id = -1;

    geo_fence_config -> tracked_object_data_subscribe_id = -1;

    if(mp_init( &(geo_fence_config -> pkt_content_mempool),
       sizeof(spkt_content), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> tracked_mac_list_head_mempool),
       sizeof(stracked_mac_list_head), SLOTS_IN_MEM_POOL) !=
              MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> rssi_list_node_mempool),
       sizeof(srssi_list_node), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> geo_fence_list_node_mempool),
       sizeof(sgeo_fence_list_node), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> uuid_list_node_mempool),
       sizeof(suuid_list_node), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> mac_prefix_list_node_mempool),
       sizeof(smac_prefix_list_node), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> geo_fence_mac_prefix_list_node_mempool),
       sizeof(sgeo_fence_mac_prefix_list_node), SLOTS_IN_MEM_POOL) !=
       MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    pthread_mutex_init(&geo_fence_config -> geo_fence_list_lock);

    if (udp_initial(&geo_fence_config -> udp_config, recv_port, api_recv_port)
        != WORK_SUCCESSFULLY)
        return E_WIFI_INIT_FAIL;

    if (startThread( &geo_fence_config -> process_api_recv_thread,
        (void *)process_api_recv, geo_fence_config) != WORK_SUCCESSFULLY){
        return E_START_THREAD;
    }

    while(geo_fence_config -> geo_fence_data_subscribe_id == -1){

        memset(content, 0, WIFI_MESSAGE_LENGTH);

        content[0] = ((from_modules << 4) & 0xf0) + (add_subscriber & 0x0f);

        sprintf(&content[1], "%s;", GEO_FENCE_TOPIC);

        udp_addpkt(udp_config, geo_fence_config -> server_ip, content,
                   strlen(content));

        last_subscribe_geo_fence_data_time = get_system_time();

        Sleep(1000); // 1 sec

    }

    while(geo_fence_config -> tracked_object_data_subscribe_id == -1){

        memset(content, 0, WIFI_MESSAGE_LENGTH);

        content[0] = ((from_modules << 4) & 0xf0) + (add_subscriber & 0x0f);

        sprintf(&content[1], "%s;", TRACKED_OBJECT_DATA_TOPIC);

        udp_addpkt(udp_config, geo_fence_config -> server_ip, content,
                   strlen(content));

        last_subscribe_tracked_object_data_time = get_system_time();

        Sleep(1000); // 1 sec

    }

    while(geo_fence_config -> is_running){

        Sleep(WAITING_TIME);

    }

    udp_release( &geo_fence_config -> udp_config);

    thpool_destroy( &geo_fence_config -> worker_thread);

    pthread_mutex_destroy( &geo_fence_config -> geo_fence_list_lock);

    mp_destroy( &geo_fence_config -> pkt_content_mempool);

    mp_destroy( &geo_fence_config -> tracked_mac_list_head_mempool);

    mp_destroy( &geo_fence_config -> rssi_list_node_mempool);

    mp_destroy( &(geo_fence_config -> geo_fence_list_node_mempool);

    mp_destroy( &(geo_fence_config -> uuid_list_node_mempool);

    mp_destroy( &(geo_fence_config -> mac_prefix_list_node_mempool);

    mp_destroy( &(geo_fence_config -> geo_fence_mac_prefix_list_node_mempool);

    return (void *)NULL;

}


static void *process_geo_fence_routine(void *_pkt_content){

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    pgeo_fence_config geo_fence_config = pkt_content -> geo_fence_config;

    ptracked_mac_list_head current_mac_list_head, tmp_mac_list_head;

    prssi_list_node current_rssi_list_node, tmp_rssi_list_node;

    char uuid[UUID_LENGTH];

    char gateway_ip[NETWORK_ADDR_LENGTH];

    int object_type;

    int number_of_objects;

    char tmp[WIFI_MESSAGE_LENGTH];

    memset(tmp, 0, WIFI_MESSAGE_LENGTH);

    memset(uuid, 0, UUID_LENGTH);

    memset(gateway_ip, 0, NETWORK_ADDR_LENGTH);

    sscanf(pkt_content->content, "%s;%s;%d;%d;%s", uuid, gateway_ip,
                                 object_type, number_of_objects, tmp);

    if (is_in_geo_fence(uuid) == NULL){
        return (void *)NULL;
    }

    do {

        for(;number_of_objects > 0;number_of_objects --){

            char tmp_data[WIFI_MESSAGE_LENGTH];

            char mac_address[LENGTH_OF_MAC_ADDRESS];

            int init_time,final_time, rssi;

            memset(tmp_data, 0, WIFI_MESSAGE_LENGTH);

            sscanf(tmp, "%s;%d;%d;%d;%s", mac_address, init_time, final_time,
                                          rssi, tmp_data);

            /* Filter Geo-Fencing */

            if (is_mac_in_geo_fence()){

            }

            if (current_mac_list_head = is_in_mac_list(geo_fence_config,
                                                       mac_address) == NULL){
                if (rssi >= geo_fence_config -> decision_threshold){

                    tmp_mac_list_head = mp_alloc(
                                 &pkt_content -> tracked_mac_list_head_mempool);

                    tmp_rssi_list_node = mp_alloc(
                                        &pkt_content -> rssi_list_node_mempool);

                    if(current_rssi_list_node = is_in_mac_list(
                       current_mac_list_head, uuid) == NULL){




                    }
                    else{





                    }

                }

            }
            else{





            }

            memset(tmp, 0, WIFI_MESSAGE_LENGTH);

            memcpy(tmp, tmp_data, strlen(tmp_data) * sizeof(char));

        }

        if (strlen(tmp) > 0){
            sscanf(tmp, "%d;%d;%s", object_type, number_of_objects, tmp_data);

            memset(tmp, 0, WIFI_MESSAGE_LENGTH);
            memcpy(tmp, tmp_data, strlen(tmp_data) * sizeof(char));
        }

    } while(strlen(tmp) > 0);

    return (void *)NULL;

}


static void *update_geo_fence(void *_pkt_content){

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    pgeo_fence_config geo_fence_config = pkt_content ->
                                         geo_fence_config;

    pgeo_fence_list_node geo_fence_list_head = &geo_fence_config ->
                                               geo_fence_head;

    pgeo_fence_list_node geo_fence_list_node;

    pmac_prefix_list_node mac_prefix_node;

    int number_of_geo_fence, geo_fence_id, counter, counter_for_lbeacon,
        counter_for_mac_prefix;

    char *name, *perimeters, *fence, *mac_prefix, *saveptr,
         *current_ptr;

    bool is_exist;

    pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

    number_of_geo_fence = -1;
    current_ptr = NULL;
    saveptr = NULL;

    current_ptr = strtok_r(pkt_content -> content,
                           DELIMITER_SEMICOLON, &saveptr);

    sscanf(current_ptr, "%d", &number_of_geo_fence);

    for (counter = 0; counter < number_of_geo_fence; counter ++){

        char *save_current_ptr, uuid, threshold_str;

        puuid_list_node uuid_list_node;
        pmac_prefix_node mac_prefix_node;
        pgeo_fence_list_node geo_fence_list_node;

        int number_of_perimeters, number_of_fence, number_of_mac_prefix,
            threshold;

        is_exist = false;
        current_ptr = NULL;
        name = NULL;
        geo_fence_list_node = NULL;
        perimeters = NULL;
        fence = NULL;
        mac_prefix = NULL;

        current_ptr = strtok_r(NULL, DELIMITER_SEMICOLON, &saveptr);

        sscanf(current_ptr, "%d", &geo_fence_id);

        name = strtok_r(NULL, DELIMITER_SEMICOLON, &saveptr);

        geo_fence_list_node = is_in_geo_fence_list(geo_fence_head,
                                                   geo_fence_id);

        if(geo_fence_list_node == NULL){
            geo_fence_list_node = mp_alloc( &geo_fence_config ->
                                           geo_fence_list_node_mempool);
        }
        else{
            free_geo_fence_list_node(geo_fence_config, geo_fence_list_node);
        }

        perimeters = strtok_r(NULL, DELIMITER_SEMICOLON,
                              &saveptr);
        fence = strtok_r(NULL, DELIMITER_SEMICOLON, &saveptr);
        mac_prefix = strtok_r(NULL, DELIMITER_SEMICOLON,
                              &saveptr);

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_r(perimeters, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_perimeters);

        for(counter_for_lbeacon = 0;counter_for_lbeacon < number_of_perimeters;
            counter_for_lbeacon ++){

            uuid = NULL;
            uuid_list_node = NULL;
            threshold_str = NULL;

            uuid = strtok_r(NULL, DELIMITER_COMMA, &save_current_ptr);

            threshold_str = strtok_r(NULL, DELIMITER_COMMA, &save_current_ptr);

            sscanf(threshold_str, "%d", &threshold);

            uuid_list_node = mp_alloc( &geo_fence_config ->
                                      uuid_list_node_mempool);

            init_uuid_list_node(uuid_list_node);

            memcpy(uuid_list_node -> uuid, uuid, UUID_LENGTH);

            uuid_list_node -> threshold = threshold;

            insert_list_tail( &uuid_list_node -> uuid_list_entry,
                             &geo_fence_list_node -> perimeters_uuid_list_head);

        }

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_r(fence, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_fence);

        for(counter_for_lbeacon = 0;counter_for_lbeacon < number_of_fence;
            counter_for_lbeacon ++){

            uuid = NULL;
            uuid_list_node = NULL;
            threshold_str = NULL;

            uuid = strtok_r(NULL, DELIMITER_COMMA, &save_current_ptr);

            threshold_str = strtok_r(NULL, DELIMITER_COMMA, &save_current_ptr);

            sscanf(threshold_str, "%d", &threshold);

            uuid_list_node = mp_alloc( &geo_fence_config ->
                                      uuid_list_node_mempool);

            init_uuid_list_node(uuid_list_node);

            memcpy(uuid_list_node -> uuid, uuid, UUID_LENGTH);

            uuid_list_node -> threshold = threshold;

            insert_list_tail( &uuid_list_node -> uuid_list_entry,
                              &geo_fence_list_node -> fence_uuid_list_head);

        }

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_r(mac_prefix, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_mac_prefix);

        for(counter_for_mac_prefix = 0;counter_for_mac_prefix <
            number_of_mac_prefix;counter_for_lbeacon ++){

            current_ptr = NULL;
            mac_prefix_node = NULL;

            current_ptr = strtok_r(NULL, DELIMITER_COMMA, &save_current_ptr);

            mac_prefix_node = mp_alloc( &geo_fence_config ->
                                       mac_prefix_list_node_mempool);

            init_mac_prefix_node(mac_prefix_node);

            memcpy(mac_prefix_node -> mac_prefix, current_ptr,
                   strlen(current_ptr) * sizeof(char));

            insert_list_tail( &mac_prefix -> mac_prefix_list_entry,
                              &geo_fence_list_node -> mac_prefix_list_head);

        }

        insert_list_tail( &geo_fence_list_node -> geo_fence_list_node,
                          &geo_fence_list_head -> geo_fence_list_entry);

    }

    pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);

    return (void *)NULL;
}


static void *process_api_recv(void *_geo_fence_config){

    int return_value;

    sPkt temppkt;

    char *tmp_addr, *current_pointer;

    ppkt_content pkt_content;

    pgeo_fence_config geo_fence_config = (pgeo_fence_config)_geo_fence_config;

    int pkt_direction, pkt_type, subscribe_id;

    char topic[WIFI_MESSAGE_LENGTH], tmp_content[WIFI_MESSAGE_LENGTH];

    while(geo_fence_config -> is_running == true){

        temppkt = udp_getrecv( &geo_fence_config -> udp_config);

        if(temppkt.type == UDP){

            pkt_direction = (temppkt.content[0] >> 4) & 0x0f;

            pkt_type = temppkt.content[0] & 0x0f;

            switch (pkt_direction) {

                case from_server:

                    switch (pkt_type) {

                        case add_subscriber:

                            memset(topic, 0, WIFI_MESSAGE_LENGTH);

                            memcpy(tmp_content, temppkt.content,
                                   sizeof(temppkt.content));

                            current_pointer = strtok(tmp_content,
                                                     DELIMITER_SEMICOLON);

                            if(current_pointer != NULL && strncmp(
                               current_pointer, GEO_FENCE_TOPIC,
                               strlen(GEO_FENCE_TOPIC)) == 0){

                                printf("TOPIC: %s\n", current_pointer);

                                current_pointer = strtok(NULL,
                                                         DELIMITER_SEMICOLON);

                                if(current_pointer != NULL && strncmp(
                                   current_pointer, "Success",
                                   strlen("Success")) == 0){

                                    current_pointer = strtok(NULL,
                                                           DELIMITER_SEMICOLON);

                                    if(current_pointer != NULL){

                                        sscanf(current_pointer, "%d",
                                               &geo_fence_config ->
                                               geo_fence_data_subscribe_id);

                                        printf("geo_fence_data_subscribe_id: "\
                                              "%d\n", geo_fence_config ->
                                              geo_fence_data_subscribe_id);

                                    }
                                }
                            }

                            else if(current_pointer != NULL && strncmp(
                               current_pointer, TRACKED_OBJECT_DATA_TOPIC,
                               strlen(TRACKED_OBJECT_DATA_TOPIC)) == 0){

                                printf("TOPIC: %s\n", current_pointer);

                                current_pointer = strtok(NULL,
                                                         DELIMITER_SEMICOLON);

                                if(current_pointer != NULL && strncmp(
                                   current_pointer, "Success",
                                   strlen("Success")) == 0){

                                    current_pointer = strtok(NULL,
                                                           DELIMITER_SEMICOLON);

                                    if(current_pointer != NULL){

                                        sscanf(current_pointer, "%d",
                                              &geo_fence_config ->
                                              tracked_object_data_subscribe_id);

                                        printf("tracked_object_data_subscribe_"\
                                               "id: %d\n", geo_fence_config ->
                                              tracked_object_data_subscribe_id);

                                    }
                                }
                            }

                            break;

                        case update_topic_data:

                            tmp_addr = udp_hex_to_address(temppkt.address);

                            pkt_content = mp_alloc(&(geo_fence_config ->
                                                   pkt_content_mempool));

                            memset(pkt_content, 0, strlen(pkt_content) *
                                   sizeof(char));

                            memcpy(pkt_content -> ip_address, tmp_addr,
                                   strlen(tmp_addr) * sizeof(char));

                            memcpy(pkt_content -> content, temppkt.content,
                                   temppkt.content_size);

                            pkt_content -> content_size = temppkt.content_size;

                            pkt_content -> geo_fence_config = geo_fence_config;

                            return_value = thpool_add_work(
                                            geo_fence_config -> worker_thread,
                                            process_geo_fence_routine,
                                            pkt_content,
                                            0);

                            free(tmp_addr);

                            break;
                    }

                    break;
            }
        }
    }

    return (void *)NULL;

}


static ErrorCode init_geo_fence_list_node(pgeo_fence_list_node geo_fence_list_node){

    geo_fence_list_node -> id = -1;

    memset(geo_fence_list_node -> name, 0, WIFI_MESSAGE_LENGTH);

    init_entry( &geo_fence_list_node -> perimeters_uuid_list_head);

    init_entry( &geo_fence_list_node -> fence_uuid_list_head);

    init_entry( &geo_fence_list_node -> mac_prefix_list_head);

    init_entry( &geo_fence_list_node -> geo_fence_head);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_geo_fence_list_node(
                                      pgeo_fence_list_node geo_fence_list_node,
                                      pgeo_fence_config geo_fence_config){

    List_Entry *current_uuid_list_ptr, *next_uuid_list_ptr,
               *current_mac_prefix_list_ptr, *next_mac_prefix_list_ptr;

    puuid_list_node current_uuid_list_node;
    pmac_prefix_node current_mac_prefix_list_node;

    current_uuid_list_ptr = NULL;
    next_uuid_list_ptr = NULL;
    current_mac_prefix_list_ptr = NULL;
    next_mac_prefix_list_ptr = NULL;

    geo_fence_list_node -> id = -1;

    memset(geo_fence_list_node -> name, 0, WIFI_MESSAGE_LENGTH);

    remove_list_node( &geo_fence_list_node -> geo_fence_list_entry);

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node ->
                       perimeters_uuid_list_head.uuid_list_entry){

        current_uuid_list_node = ListEntry(current_uuid_list_ptr,
                                           suuid_list_node,
                                           uuid_list_entry);

        remove_list_node(current_uuid_list_ptr);

        mp_free( &geo_fence_config -> uuid_list_node_mempool,
                current_list_node);

    }

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node ->
                       fence_uuid_list_head.uuid_list_entry){

        current_uuid_list_node = ListEntry(current_uuid_list_ptr,
                                           suuid_list_node,
                                           uuid_list_entry);

        remove_list_node(current_uuid_list_ptr);

        mp_free( &geo_fence_config -> uuid_list_node_mempool,
                current_list_node);

    }

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node ->
                       mac_prefix_list_head.mac_prefix_list_entry){

        current_mac_prefix_list_node = ListEntry(current_uuid_list_ptr,
                                                 smac_prefix_list_node,
                                                 mac_prefix_list_entry);

        remove_list_node(current_uuid_list_ptr);

        mp_free( &geo_fence_config -> mac_prefix_list_node_mempool,
                current_list_node);

    }

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_tracked_mac_list_node(ptracked_mac_list_head tracked_mac_list_head){

    init_entry(&tracked_mac_list_head -> mac_list_entry);

    init_entry(tracked_mac_list_head -> rssi_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_tracked_mac_list_node(ptracked_mac_list_head tracked_mac_list_head){

    remove_list_node(&tracked_mac_list_head -> mac_list_entry);

    remove_list_node(tracked_mac_list_head -> rssi_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_rssi_list_node(prssi_list_node rssi_list_node){

    memset(rssi_list_node -> uuid, 0, UUID_LENGTH);

    rssi_list_node -> rssi = 0;

    rssi_list_node -> first_time = 0;

    rssi_list_node -> last_time = 0;

    init_entry(&rssi_list_node -> rssi_list_node);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_rssi_list_node(prssi_list_node rssi_list_node){

    memset(rssi_list_node -> uuid, 0, UUID_LENGTH);

    rssi_list_node -> rssi = 0;

    rssi_list_node -> first_time = 0;

    rssi_list_node -> last_time = 0;

    remove_list_node(&rssi_list_node -> rssi_list_node);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_uuid_list_node(puuid_list_node uuid_list_node){

    memset(uuid_list_node -> uuid, 0, UUID_LENGTH);

    init_entry( &uuid_list_node -> uuid_list_entry);

    init_entry( &uuid_list_node -> geo_fence_perimeter_uuid_list_entry);

    init_entry( &uuid_list_node -> geo_fence_fence_uuid_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_uuid_list_node(puuid_list_node uuid_list_node){

    memset(uuid_list_node -> uuid, 0, UUID_LENGTH);

    remove_list_node( &uuid_list_node -> uuid_list_entry);

    remove_list_node( &uuid_list_node -> geo_fence_perimeter_uuid_list_entry);

    remove_list_node( &uuid_list_node -> geo_fence_fence_uuid_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_mac_prefix_node(pmac_prefix_node mac_prefix_node){

    memset(mac_prefix_node -> mac_prefix, 0, LENGTH_OF_MAC_ADDRESS);

    init_entry( &mac_prefix_node -> mac_prefix_list_entry);

    init_entry( &mac_prefix_node -> geo_fence_mac_prefix_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_mac_prefix_node(pmac_prefix_node mac_prefix_node){

    memset(mac_prefix_node -> mac_prefix, 0, LENGTH_OF_MAC_ADDRESS);

    remove_list_node( &mac_prefix_node -> mac_prefix_list_entry);

    remove_list_node( &mac_prefix_node -> geo_fence_mac_prefix_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_pgeo_fence_mac_prefix_list_node(
                pgeo_fence_mac_prefix_list_node geo_fence_mac_prefix_list_node){

    init_entry( &geo_fence_mac_prefix_list_node ->
               geo_fence_mac_prefix_list_entry);

    init_entry( &geo_fence_mac_prefix_list_node ->
               geo_fence_mac_prefix_list_node_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_pgeo_fence_mac_prefix_list_node(
                pgeo_fence_mac_prefix_list_node geo_fence_mac_prefix_list_node){

    remove_list_node( &geo_fence_mac_prefix_list_node ->
                     geo_fence_mac_prefix_list_entry);

    remove_list_node( &geo_fence_mac_prefix_list_node ->
                     geo_fence_mac_prefix_list_node_entry);

    return WORK_SUCCESSFULLY;

}


static pgeo_fence_list_node is_in_geo_fence_list(
                        pgeo_fence_list_node geo_fence_list, int geo_fence_id){

    List_Entry *current_list_ptr;
    pgeo_fence_list_node current_geo_fence_list_node;

    list_for_each(current_list_ptr, &geo_fence_list -> geo_fence_list_head){

        current_geo_fence_list_node = ListEntry(current_list_ptr,
                                                sgeo_fence_list_node,
                                                geo_fence_list_entry);

        if(current_geo_fence_list_node -> id == geo_fence_id){

            return current_geo_fence_list_node;

        }
    }

    return NULL;

}


static puuid_list_node is_in_geo_fence(char *uuid){


    return NULL;


}


static ptracked_mac_list_head is_in_mac_list(pgeo_fence_config geo_fence_config,
                                             char *mac_address){

    int return_value;

    List_Entry *mac_entry_pointer,  *next_mac_entry_pointer;

    ptracked_mac_list_head current_mac_list_head;

    list_for_each_safe(mac_entry_pointer, next_mac_entry_pointer,
                       &geo_fence_config -> ptracked_mac_list_head){

        current_mac_list_head = ListEntry(mac_entry_pointer,
                                          stracked_mac_list_head,
                                          mac_list_entry);

        return_value = strncmp(mac_address,
                               current_mac_list_head -> mac_address,
                               LENGTH_OF_MAC_ADDRESS);

        if (return_value == 0)
            return current_mac_list_head;

    }

    return NULL;

}


static prssi_list_node is_in_rssi_list(
                                   ptracked_mac_list_head tracked_mac_list_head,
                                   char *uuid){

    int return_value;

    List_Entry *rssi_entry_pointer, *next_rssi_entry_pointer;

    prssi_list_node current_rssi_list_node;

    list_for_each_safe(rssi_entry_pointer, next_rssi_entry_pointer,
                       &tracked_mac_list_head -> rssi_list_head){

        current_rssi_list_node = ListEntry(rssi_entry_pointer,
                                           srssi_list_node,
                                           rssi_list_node);

        return_value = strncmp(uuid,
                               current_rssi_list_node -> uuid,
                               UUID_LENGTH);

        if (return_value == 0)
            return current_rssi_list_node;

    }

    return NULL;

}
