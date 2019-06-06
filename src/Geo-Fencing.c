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

     1.0, 20190606

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

     Chun Yu Lai  , chunyu1202@gmail.com
     Gary Xiao    , garyh0205@hotmail.com

 */

#include "Geo-Fencing.h"


void *geo_fence_routine(void *_geo_fence_config){

    pgeo_fence_config geo_fence_config = (pgeo_fence_config)_geo_fence_config;

    pudp_config udp_config = &geo_fence_config -> udp_config;

    int last_subscribe_geo_fence_data_time,
        last_subscribe_tracked_object_data_time;

    char content[WIFI_MESSAGE_LENGTH];

    geo_fence_config -> is_running = true;

    geo_fence_config -> geo_fence_data_subscribe_id = -1;

    geo_fence_config -> tracked_object_data_subscribe_id = -1;

    geo_fence_config -> geo_fence_alert_data_owner_id = -1;

#ifdef debugging
    printf("[GeoFence] Allicating Memory\n");
#endif

    if(mp_init( &(geo_fence_config -> pkt_content_mempool),
       sizeof(spkt_content), SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    if(mp_init( &(geo_fence_config -> tracked_mac_list_head_mempool),
       sizeof(smac_prefix_list_node), SLOTS_IN_MEM_POOL) !=
              MEMORY_POOL_SUCCESS)
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

    init_entry( &(geo_fence_config -> geo_fence_list_head.geo_fence_list_entry))
    ;

#ifdef debugging
    printf("[GeoFence] Allicating Memory success\n");
#endif

    pthread_mutex_init(&geo_fence_config -> geo_fence_list_lock, 0);

#ifdef debugging
    printf("[GeoFence] init UDP\n");
#endif

    if (udp_initial(&geo_fence_config -> udp_config, geo_fence_config ->
        api_recv_port, geo_fence_config -> recv_port) != WORK_SUCCESSFULLY)
        return E_WIFI_INIT_FAIL;

#ifdef debugging
    printf("[GeoFence] init UDP success\n");
#endif

#ifdef debugging
    printf("[GeoFence] init api receiving thread\n");
#endif

    if (startThread( &geo_fence_config -> process_api_recv_thread,
        (void *)process_api_recv, geo_fence_config) != WORK_SUCCESSFULLY){
        return E_START_THREAD;
    }

#ifdef debugging
    printf("[GeoFence] init api receiving thread success\n");
#endif

    while(geo_fence_config -> geo_fence_data_subscribe_id == -1){

#ifdef debugging
        printf("[GeoFence] Init GeoFence\n");
#endif

        memset(content, 0, WIFI_MESSAGE_LENGTH);

        content[0] = ((from_modules << 4) & 0xf0) + (add_subscriber & 0x0f);

        sprintf( &content[1], "%s;%s;", GEO_FENCE_TOPIC,
                geo_fence_config -> server_ip);

        udp_addpkt(udp_config, geo_fence_config -> server_ip, content,
                   strlen(content));

        last_subscribe_geo_fence_data_time = get_system_time();

        Sleep(3000); // 3 sec

    }

#ifdef debugging
    printf("[GeoFence] Init GeoFence success\n");
#endif

#ifdef debugging
    printf("[GeoFence] Subscribe tracked object data\n");
#endif

    while(geo_fence_config -> tracked_object_data_subscribe_id == -1){

        memset(content, 0, WIFI_MESSAGE_LENGTH);

        content[0] = ((from_modules << 4) & 0xf0) + (add_subscriber & 0x0f);

        sprintf( &content[1], "%s;%s;", TRACKED_OBJECT_DATA_TOPIC,
                geo_fence_config -> server_ip);

        udp_addpkt(udp_config, geo_fence_config -> server_ip, content,
                   strlen(content));

        last_subscribe_tracked_object_data_time = get_system_time();

        Sleep(3000); // 3 sec

    }

#ifdef debugging
    printf("[GeoFence] Subscribe tracked object data Success\n");
#endif

#ifdef debugging
    printf("[GeoFence] Register geo fence alert\n");
#endif

    while(geo_fence_config -> geo_fence_alert_data_owner_id == -1){

        memset(content, 0, WIFI_MESSAGE_LENGTH);

        content[0] = ((from_modules << 4) & 0xf0) + (add_data_owner & 0x0f);

        sprintf( &content[1], "%s;%s;", GEO_FENCE_ALERT_TOPIC,
                geo_fence_config -> server_ip);

        udp_addpkt(udp_config, geo_fence_config -> server_ip, content,
                   strlen(content));

        last_subscribe_tracked_object_data_time = get_system_time();

        Sleep(3000); // 3 sec

    }

#ifdef debugging
    printf("[GeoFence] Register geo fence alert success\n");
#endif

    while(geo_fence_config -> is_running){

        Sleep(WAITING_TIME);

    }

    udp_release( &geo_fence_config -> udp_config);

    pthread_mutex_destroy( &geo_fence_config -> geo_fence_list_lock);

    mp_destroy( &geo_fence_config -> pkt_content_mempool);

    mp_destroy( &geo_fence_config -> tracked_mac_list_head_mempool);

    mp_destroy( &geo_fence_config -> rssi_list_node_mempool);

    mp_destroy( &geo_fence_config -> geo_fence_list_node_mempool);

    mp_destroy( &geo_fence_config -> uuid_list_node_mempool);

    mp_destroy( &geo_fence_config -> mac_prefix_list_node_mempool);

    mp_destroy( &geo_fence_config -> geo_fence_mac_prefix_list_node_mempool);

    return (void *)NULL;

}


static void *process_geo_fence_routine(void *_pkt_content){

    /* lbeacon_uuid;lbeacon_datetime,lbeacon_ip;
     * object_type;object_number;object_mac_address_1;initial_timestamp_GMT_1;
     * final_timestamp_GMT_1;rssi_1;panic_button_1;object_type;object_number;
     * object_mac_address_2;initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2
     * ;panic_button_2;
     */

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    pgeo_fence_config geo_fence_config = pkt_content -> geo_fence_config;

    ppkt_content pkt_content_to_process;

    pgeo_fence_list_node current_list_ptr;

    puuid_list_node current_uuid_list_ptr;

    List_Entry *current_list_entry, *current_uuid_list_entry;

    char *save_ptr, *uuid, *lbeacon_ip, *lbeacon_datetime, *topic_name;

    char content_temp[WIFI_MESSAGE_LENGTH];

    int return_value;

    current_list_ptr = NULL;
    save_ptr = NULL;
    topic_name = NULL;

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);

    memcpy(content_temp, pkt_content -> content, WIFI_MESSAGE_LENGTH);

    topic_name = strtok_save( &content_temp[1], DELIMITER_SEMICOLON, &save_ptr);

    if(topic_name != NULL && strncmp(topic_name, GEO_FENCE_TOPIC,
       strlen(GEO_FENCE_TOPIC)) == 0){

        update_geo_fence(pkt_content);

    }
    else if(topic_name != NULL && strncmp(topic_name, TRACKED_OBJECT_DATA_TOPIC,
            strlen(TRACKED_OBJECT_DATA_TOPIC)) == 0){

        uuid = strtok_save( NULL, DELIMITER_SEMICOLON, &save_ptr);

#ifdef debugging
        printf("[GeoFence] Current processing UUID: %s\n", uuid);
#endif

        lbeacon_datetime = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

        lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

        pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

        list_for_each(current_list_entry, &geo_fence_config ->
                      geo_fence_list_head.geo_fence_list_entry){

            current_list_ptr = ListEntry(current_list_entry,
                                         sgeo_fence_list_node,
                                         geo_fence_list_entry);

            current_uuid_list_entry = NULL;

            list_for_each(current_uuid_list_entry, &current_list_ptr ->
                          fence_uuid_list_head){

                current_uuid_list_ptr = ListEntry(current_uuid_list_entry,
                                                  suuid_list_node,
                                                  uuid_list_entry);

                if(strncmp(uuid, current_uuid_list_ptr -> uuid, UUID_LENGTH) ==
                   0){

                    pkt_content_to_process = mp_alloc( &(geo_fence_config ->
                                                      pkt_content_mempool));
                    if(pkt_content_to_process != NULL){
                        sprintf(pkt_content_to_process -> content, 
                                                "%d;%s;%d;%s", 
                                                current_list_ptr -> id, 
                                                "F",
                                                current_uuid_list_ptr -> 
                                                threshold,
                                                 &pkt_content->content[1]);

#ifdef debugging
                        printf("[GeoFence] Content: %s\n", 
                                             pkt_content_to_process -> content);
#endif

                        pkt_content_to_process -> content_size = strlen(
                                             pkt_content_to_process -> content);

#ifdef debugging
                        printf("[GeoFence] Size: %d\n", 
                                       pkt_content_to_process ->  content_size);
#endif

                        pkt_content_to_process -> geo_fence_config =
                                                               geo_fence_config;

                        memcpy(pkt_content_to_process -> ip_address, 
                                                      pkt_content -> ip_address,
                                                      NETWORK_ADDR_LENGTH);

                        check_tracking_object_data_routine
                                                       (pkt_content_to_process);

                        mp_free(&(geo_fence_config -> pkt_content_mempool),  
                                                        pkt_content_to_process);
                    }
                    break;
                }
            }

            current_uuid_list_entry = NULL;

            list_for_each(current_uuid_list_entry, &current_list_ptr ->
                                                     perimeters_uuid_list_head){

                current_uuid_list_ptr = ListEntry(current_uuid_list_entry,
                                                  suuid_list_node,
                                                  uuid_list_entry);

                if(strncmp(uuid, current_uuid_list_ptr -> uuid, UUID_LENGTH) ==
                   0){

                    pkt_content_to_process = mp_alloc(&(geo_fence_config ->
                                                        pkt_content_mempool));
                    if(pkt_content_to_process != NULL){
                        sprintf(pkt_content_to_process -> content, 
                                                       "%d;%s;%d;%s",
                                                       current_list_ptr->id, 
                                                       "P", 
                                                       current_uuid_list_ptr ->
                                                       threshold, 
                                                        &pkt_content->content[1]
                                                       );

#ifdef debugging
                        printf("[GeoFence] Content: %s\n",
                                             pkt_content_to_process -> content);
#endif

                        pkt_content_to_process -> content_size = 
                                        strlen(pkt_content_to_process->content);

#ifdef debugging
                        printf("[GeoFence] Size: %d\n", 
                                        pkt_content_to_process -> content_size);
#endif

                        pkt_content_to_process -> geo_fence_config =
                                                               geo_fence_config;

                        memcpy(pkt_content_to_process->ip_address, 
                                pkt_content -> ip_address, NETWORK_ADDR_LENGTH);

                        check_tracking_object_data_routine
                                                       (pkt_content_to_process);
                        mp_free(&(geo_fence_config -> pkt_content_mempool),  
                                                        pkt_content_to_process);
                    }
                    break;
                }
            }
        }

        pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);
    }

    return (void *)NULL;

}


static void *check_tracking_object_data_routine(void *_pkt_content){

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    pgeo_fence_config geo_fence_config = pkt_content -> geo_fence_config;

	pudp_config udp_config = &geo_fence_config -> udp_config;

    List_Entry *current_list_entry, *current_mac_list_entry;

    sgeo_fence_list_node *current_list_ptr, *geo_fence_list_ptr;

    smac_prefix_list_node *current_mac_prefix_list_ptr;

    char *current_ptr, *saved_ptr, *uuid, *lbeacon_ip, *lbeacon_type,
         *mac_address, *initial_timestamp, *final_timestamp, *panic_button;

    char content_for_processing[WIFI_MESSAGE_LENGTH];

    int geo_fence_id, object_type, number_of_objects, rssi, threshold;

    char content_for_alert[WIFI_MESSAGE_LENGTH];

    /*
     * id;"P/F";threshold;tracking_data
     */

    memset(content_for_processing, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_for_processing, pkt_content->content, WIFI_MESSAGE_LENGTH);
    geo_fence_list_ptr  = NULL;

    current_ptr = strtok_save(content_for_processing, DELIMITER_SEMICOLON,
                           &saved_ptr);

    sscanf(current_ptr, "%d", &geo_fence_id);

    list_for_each(current_list_entry, &geo_fence_config ->
                                      geo_fence_list_head.geo_fence_list_entry){

        geo_fence_list_ptr = ListEntry(current_list_entry,
                                       sgeo_fence_list_node,
                                       geo_fence_list_entry);

#ifdef debugging
        printf("[GeoFence] geo_fence_list_ptr -> id: %d\n", 
                                                      geo_fence_list_ptr -> id);
#endif

        if(geo_fence_list_ptr -> id == geo_fence_id){
            break;
        }

    }

    if(geo_fence_list_ptr == NULL){

#ifdef debugging
        printf("[GeoFence] Exit\n");
#endif

        return (void *)NULL;
    }

#ifdef debugging
    printf("[GeoFence] Start detect mac address\n");
#endif

    lbeacon_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

#ifdef debugging
    printf("[GeoFence] LBeacon Fence Type: %s\n", lbeacon_type);
#endif

    current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

    sscanf(current_ptr, "%d", &threshold);

#ifdef debugging
    printf("[GeoFence] Threshold: %d\n", threshold);
#endif

    current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

    uuid = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

#ifdef debugging
    printf("[GeoFence] UUID: %s\n", uuid);
#endif

    current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

#ifdef debugging
    printf("[GeoFence] LBeacon IP: %s\n", lbeacon_ip);
#endif

    // Start processing received tracked object data

    do{

        current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

        sscanf(current_ptr, "%d", &object_type);

#ifdef debugging
        printf("[GeoFence] Object Type: %d\n", object_type);
#endif

        current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

        sscanf(current_ptr, "%d", &number_of_objects);

#ifdef debugging
        printf("[GeoFence] Number of Objects: %d\n", number_of_objects);
#endif

        while(number_of_objects --){

            //mac_address;initial_timestamp;final_timestamp;rssi;panic_button;

            mac_address = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            initial_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            final_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            sscanf(current_ptr, "%d", &rssi);

            panic_button = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            list_for_each(current_mac_list_entry, &geo_fence_list_ptr ->
                                                          mac_prefix_list_head){

                current_mac_prefix_list_ptr = ListEntry(current_mac_list_entry,
                                                        smac_prefix_list_node,
                                                        mac_prefix_list_entry);

                if((strncmp_caseinsensitive(mac_address,
                        current_mac_prefix_list_ptr -> mac_prefix,
                        strlen(current_mac_prefix_list_ptr -> mac_prefix)) == 0)
                        && rssi >= threshold){

//#ifdef debugging
                    printf("[GeoFence-Alert] timestamp %d - %s %s %s %d\n",
                           get_system_time(), uuid, lbeacon_ip, mac_address,
                           rssi);

                    memset(content_for_alert, 0, WIFI_MESSAGE_LENGTH);

                    content_for_alert[0] =  ((from_modules << 4) & 0xf0) +
                                             (update_topic_data & 0x0f);

                    //number_of_geo_fence_alert;mac_address1;type1;uuid1;
                    //alert_time1;rssi1;geo_fence_id1;

                    sprintf( &content_for_alert[1], "%s;%d;%s;%s;%s;%s;%d;%d;",
                            GEO_FENCE_ALERT_TOPIC, 1, mac_address, lbeacon_type,
                            uuid, final_timestamp, rssi, geo_fence_id);

                    udp_addpkt(udp_config, geo_fence_config -> server_ip,
                               content_for_alert, strlen(content_for_alert));


//#endif

                }
            }

        }

    }while(object_type != (max_type - 1));

    return (void *)NULL;

}


static void *update_geo_fence(void *_pkt_content){

    ppkt_content pkt_content = (ppkt_content)_pkt_content;

    pgeo_fence_config geo_fence_config = pkt_content ->
                                         geo_fence_config;

    pgeo_fence_list_node geo_fence_list_head = &geo_fence_config ->
                                               geo_fence_list_head;

    pgeo_fence_list_node geo_fence_list_node;
    pgeo_fence_list_node current_list_ptr;

    pmac_prefix_list_node mac_prefix_node;

    List_Entry *current_list_entry;

    int number_of_geo_fence, geo_fence_id, counter, counter_for_lbeacon,
        counter_for_mac_prefix;

    char *name, *perimeters, *fence, *mac_prefix, *saveptr,
         *current_ptr;

    bool is_exist;

    pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

    number_of_geo_fence = -1;
    current_ptr = NULL;
    saveptr = NULL;

    current_ptr = strtok_save( &pkt_content -> content[1],
                           DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
    printf("[GeoFence] Topic: %s\n", current_ptr);
#endif

    current_ptr = strtok_save( NULL,
                           DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
    printf("[GeoFence] number_of_geo_fence: %s\n", current_ptr);
#endif

    sscanf(current_ptr, "%d", &number_of_geo_fence);

    for (counter = 0; counter < number_of_geo_fence; counter ++){

        char *save_current_ptr, *uuid, *threshold_str;

        puuid_list_node uuid_list_node;
        pmac_prefix_list_node mac_prefix_node;
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

        current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        sscanf(current_ptr, "%d", &geo_fence_id);

#ifdef debugging
        printf("[GeoFence] Current Proceccing Geo Fence ID: %d\n", geo_fence_id)
        ;
#endif

        name = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
        printf("[GeoFence] Current Proceccing Geo Fence NAME: %s\n", name);
#endif

        list_for_each(current_list_entry, &geo_fence_config ->
                                      geo_fence_list_head.geo_fence_list_entry){

            current_list_ptr = ListEntry(current_list_entry,
                                         sgeo_fence_list_node,
                                         geo_fence_list_entry);

#ifdef debugging
            printf("[GeoFence] current_list_ptr->id: %d\n[GeoFence] "\
                   "geo_fence_id: %d\n", current_list_ptr->id, geo_fence_id);
#endif

            if(current_list_ptr->id == geo_fence_id){

                geo_fence_list_node = current_list_ptr;

                break;
            }

        }

        if(geo_fence_list_node == NULL){
            geo_fence_list_node = mp_alloc( &geo_fence_config ->
                                           geo_fence_list_node_mempool);

            if(geo_fence_list_node != NULL){
                init_geo_fence_list_node(geo_fence_list_node);

    #ifdef debugging
                printf("[GeoFence] Not Exists\n");
    #endif
            }
        }
        else{
            free_geo_fence_list_node(geo_fence_list_node, geo_fence_config);

#ifdef debugging
            printf("[GeoFence] Exists\n");
#endif
        }

        perimeters = strtok_save(NULL, DELIMITER_SEMICOLON,
                              &saveptr);

        geo_fence_list_node->id = geo_fence_id;

        memcpy(geo_fence_list_node->name, name, strlen(name) * sizeof(char));

#ifdef debugging
        printf("[GeoFence] Perimeters: [%s]\n", perimeters);
#endif

        fence = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
        printf("[GeoFence] Fence: [%s]\n", fence);
#endif

        mac_prefix = strtok_save(NULL, DELIMITER_SEMICOLON,
                              &saveptr);

#ifdef debugging
        printf("[GeoFence] Mac_Prefix: [%s]\n", mac_prefix);
#endif

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_save(perimeters, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_perimeters);

#ifdef debugging
        printf("[GeoFence] number_of_perimeters: %d\n", number_of_perimeters);
#endif

        for(counter_for_lbeacon = 0;counter_for_lbeacon < number_of_perimeters;
            counter_for_lbeacon ++){

            uuid = NULL;
            uuid_list_node = NULL;
            threshold_str = NULL;

            uuid = strtok_save(NULL, DELIMITER_COMMA, &save_current_ptr);

#ifdef debugging
            printf("[GeoFence] UUID: %s\n", uuid);
#endif
            threshold_str = strtok_save(NULL, DELIMITER_COMMA, &save_current_ptr);

            sscanf(threshold_str, "%d", &threshold);

#ifdef debugging
            printf("[GeoFence] Threshold: %d\n", threshold);
#endif

            uuid_list_node = mp_alloc( &geo_fence_config ->
                                      uuid_list_node_mempool);
            if(uuid_list_node != NULL){
                init_uuid_list_node(uuid_list_node);

                memcpy(uuid_list_node -> uuid, uuid, UUID_LENGTH);

    #ifdef debugging
                printf("[GeoFence] uuid_list_node -> uuid: %s\n", 
                                                     uuid_list_node -> uuid);
    #endif
                uuid_list_node -> threshold = threshold;

    #ifdef debugging
                printf("[GeoFence] uuid_list_node -> threshold: %d\n",
                                                uuid_list_node -> threshold);
    #endif

                insert_list_tail( &uuid_list_node -> uuid_list_entry,
                                  &geo_fence_list_node -> 
                                 perimeters_uuid_list_head);
            }
        }

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_save(fence, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_fence);

#ifdef debugging
        printf("[GeoFence] number_of_fence: %d\n", number_of_fence);
#endif

        for(counter_for_lbeacon = 0;counter_for_lbeacon < number_of_fence;
            counter_for_lbeacon ++){

            uuid = NULL;
            uuid_list_node = NULL;
            threshold_str = NULL;

            uuid = strtok_save(NULL, DELIMITER_COMMA, &save_current_ptr);

#ifdef debugging
            printf("[GeoFence] UUID: %s\n", uuid);
#endif

            threshold_str = strtok_save(NULL, DELIMITER_COMMA, &save_current_ptr);

            sscanf(threshold_str, "%d", &threshold);

#ifdef debugging
            printf("[GeoFence] Threshold: %d\n", threshold);
#endif

            uuid_list_node = mp_alloc( &geo_fence_config ->
                                      uuid_list_node_mempool);
            if(uuid_list_node != NULL){
                init_uuid_list_node(uuid_list_node);

                memcpy(uuid_list_node -> uuid, uuid, UUID_LENGTH);

                uuid_list_node -> threshold = threshold;

                insert_list_tail( &uuid_list_node -> uuid_list_entry,
                                  &geo_fence_list_node -> fence_uuid_list_head);
            }
        }

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_save(mac_prefix, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_mac_prefix);

#ifdef debugging
        printf("[GeoFence] number_of_mac_prefix: %d\n", number_of_mac_prefix);
#endif

        for(counter_for_mac_prefix = 0;counter_for_mac_prefix <
            number_of_mac_prefix;counter_for_mac_prefix ++){

            current_ptr = NULL;
            mac_prefix_node = NULL;

            current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_current_ptr);

            mac_prefix_node = mp_alloc( &geo_fence_config ->
                                       mac_prefix_list_node_mempool);
            if(mac_prefix_node != NULL){
                init_mac_prefix_node(mac_prefix_node);

                memcpy(mac_prefix_node -> mac_prefix, current_ptr,
                       strlen(current_ptr) * sizeof(char));
    #ifdef debugging
                printf("[GeoFence] MAC_Prefix: %s\n", mac_prefix_node -> 
                                                      mac_prefix)
                ;
    #endif
                insert_list_tail( &mac_prefix_node -> mac_prefix_list_entry,
                                  &geo_fence_list_node -> mac_prefix_list_head);

            }
        }

        insert_list_tail( &geo_fence_list_node -> geo_fence_list_entry,
                          &geo_fence_list_head -> geo_fence_list_entry);

    }

    pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);

    return (void *)NULL;
}


static void *process_api_recv(void *_geo_fence_config){

    int return_value;

    sPkt temppkt;

    char tmp_addr[NETWORK_ADDR_LENGTH];
    char *current_pointer, *saved_ptr, *topic;

    ppkt_content pkt_content;

    pgeo_fence_config geo_fence_config = (pgeo_fence_config)_geo_fence_config;

    int pkt_direction, pkt_type, subscribe_id;

    char tmp_content[WIFI_MESSAGE_LENGTH];

    current_pointer = NULL;
    saved_ptr = NULL;

    while(geo_fence_config -> is_running == true){

        temppkt = udp_getrecv( &geo_fence_config -> udp_config);

        memcpy( &tmp_content,  &temppkt.content, WIFI_MESSAGE_LENGTH);

        if(temppkt.type == UDP){

            pkt_direction = (temppkt.content[0] >> 4) & 0x0f;

            pkt_type = temppkt.content[0] & 0x0f;

#ifdef debugging
            printf("[GeoFence] get pkts\nDirection: %d\nType: %d\n",
                                                       pkt_direction, pkt_type);
#endif

            switch (pkt_direction) {

                case from_server:

                    switch (pkt_type) {

                        case add_data_owner:
#ifdef debugging
                            printf("[GeoFence] pkt content in responsed "\
                                   "add topic: [%s]\n", tmp_content);
#endif
                            topic = strtok_save( &tmp_content[1],
                                           DELIMITER_SEMICOLON, &saved_ptr);

#ifdef debugging
                            printf("[GeoFence] Topic: %s\n", topic);

                            if(topic != NULL && strncmp(
                               topic, GEO_FENCE_ALERT_TOPIC,
                               strlen(GEO_FENCE_ALERT_TOPIC)) == 0){

                                current_pointer = strtok_save(NULL,
                                                         DELIMITER_SEMICOLON,
                                                         &saved_ptr);

                                if(current_pointer != NULL && strncmp(
                                   current_pointer, "Success",
                                   strlen("Success")) == 0){

                                    current_pointer = strtok_save(NULL,
                                                           DELIMITER_SEMICOLON,
                                                           &saved_ptr);

                                    if(current_pointer != NULL){

                                        sscanf(current_pointer, "%d",
                                              &geo_fence_config ->
                                              geo_fence_alert_data_owner_id);

#ifdef debugging
                                        printf("[GeoFence] geo_fence_alert_" \
                                               "data_owner_id: %d\n",
                                               geo_fence_config ->
                                              geo_fence_alert_data_owner_id);
#endif
                                    }
                                }
                            }
#endif

                            break;

                        case add_subscriber:

#ifdef debugging
                            printf("[GeoFence] pkt content in responsed "\
                                   "add subscriber: [%s]\n", tmp_content);
#endif

                            topic = strtok_save( &tmp_content[1],
                                              DELIMITER_SEMICOLON,
                                              &saved_ptr);

#ifdef debugging
                            printf("[GeoFence] TOPIC: %s\n", topic);
#endif

                            if(topic != NULL && strncmp(
                               topic, TRACKED_OBJECT_DATA_TOPIC,
                               strlen(TRACKED_OBJECT_DATA_TOPIC)) == 0){

                                current_pointer = strtok_save(NULL,
                                                         DELIMITER_SEMICOLON,
                                                         &saved_ptr);

                                if(current_pointer != NULL && strncmp(
                                   current_pointer, "Success",
                                   strlen("Success")) == 0){

                                    current_pointer = strtok_save(NULL,
                                                           DELIMITER_SEMICOLON,
                                                           &saved_ptr);

                                    if(current_pointer != NULL){

                                        sscanf(current_pointer, "%d",
                                              &geo_fence_config ->
                                              tracked_object_data_subscribe_id);

#ifdef debugging
                                        printf("[GeoFence] tracked_object_data"\
                                               "_subscribe_id: %d\n",
                                               geo_fence_config ->
                                              tracked_object_data_subscribe_id);
#endif
                                    }
                                }
                            }

                            if(topic != NULL && strncmp(
                               topic, GEO_FENCE_TOPIC,
                               strlen(GEO_FENCE_TOPIC)) == 0){

                                current_pointer = strtok_save(NULL,
                                                         DELIMITER_SEMICOLON,
                                                         &saved_ptr);

                                if(current_pointer != NULL && strncmp(
                                   current_pointer, "Success",
                                   strlen("Success")) == 0){

                                    current_pointer = strtok_save(NULL,
                                                           DELIMITER_SEMICOLON,
                                                           &saved_ptr);

                                    if(current_pointer != NULL){

                                        sscanf(current_pointer, "%d",
                                               &geo_fence_config ->
                                               geo_fence_data_subscribe_id);
#ifdef debugging
                                        printf("geo_fence_data_subscribe_id: "\
                                              "%d\n", geo_fence_config ->
                                              geo_fence_data_subscribe_id);
#endif
                                    }
                                }
                            }

                            break;

                        case update_topic_data:


#ifdef debugging
                            printf("[GeoFence] pkt content for update data: "\
                                   "[%s]\n", tmp_content);
#endif

                            topic = strtok_save( &tmp_content[1],
                                                     DELIMITER_SEMICOLON,
                                                     &saved_ptr);

#ifdef debugging
                            printf("[GeoFence] Topic: %s\n", topic);
#endif
                            memset(tmp_addr, 0, sizeof(tmp_addr));
                            udp_hex_to_address(temppkt.address, tmp_addr);

                            pkt_content = mp_alloc(&(geo_fence_config ->
                                                   pkt_content_mempool));

                            if(pkt_content != NULL){

							    memset(pkt_content, 0, sizeof(pkt_content));

                                memcpy(pkt_content -> ip_address, tmp_addr,
                                       strlen(tmp_addr) * sizeof(char));

                                memcpy(pkt_content -> content, temppkt.content,
                                       temppkt.content_size);

                                pkt_content -> content_size = 
                                                           temppkt.content_size;

                                pkt_content -> geo_fence_config = 
                                                               geo_fence_config;
   
                                process_geo_fence_routine(pkt_content);
							    mp_free( &geo_fence_config->pkt_content_mempool,
                                        pkt_content);

                            }
                            break;
                    }

                    break;
            }
        }
    }

    return (void *)NULL;

}


static ErrorCode init_geo_fence_list_node(pgeo_fence_list_node
                                                           geo_fence_list_node){

    geo_fence_list_node -> id = -1;

    memset(geo_fence_list_node -> name, 0, WIFI_MESSAGE_LENGTH);

    init_entry( &geo_fence_list_node -> perimeters_uuid_list_head);

    init_entry( &geo_fence_list_node -> fence_uuid_list_head);

    init_entry( &geo_fence_list_node -> mac_prefix_list_head);

    init_entry( &geo_fence_list_node -> geo_fence_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_geo_fence_list_node(
                                      pgeo_fence_list_node geo_fence_list_node,
                                      pgeo_fence_config geo_fence_config){

    List_Entry *current_uuid_list_ptr, *next_uuid_list_ptr,
               *current_mac_prefix_list_ptr, *next_mac_prefix_list_ptr;

    puuid_list_node current_uuid_list_node;
    pmac_prefix_list_node current_mac_prefix_list_node;

    current_uuid_list_ptr = NULL;
    next_uuid_list_ptr = NULL;
    current_mac_prefix_list_ptr = NULL;
    next_mac_prefix_list_ptr = NULL;

    geo_fence_list_node -> id = -1;

    memset(geo_fence_list_node -> name, 0, WIFI_MESSAGE_LENGTH);

    remove_list_node( &geo_fence_list_node -> geo_fence_list_entry);

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node -> perimeters_uuid_list_head){

        current_uuid_list_node = ListEntry(current_uuid_list_ptr,
                                           suuid_list_node,
                                           uuid_list_entry);

        free_uuid_list_node(current_uuid_list_node);

        mp_free( &geo_fence_config -> uuid_list_node_mempool,
                current_uuid_list_node);

    }

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node -> fence_uuid_list_head){

        current_uuid_list_node = ListEntry(current_uuid_list_ptr,
                                           suuid_list_node,
                                           uuid_list_entry);

        free_uuid_list_node(current_uuid_list_node);

        mp_free( &geo_fence_config -> uuid_list_node_mempool,
                current_uuid_list_node);

    }

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node -> mac_prefix_list_head){

        current_mac_prefix_list_node = ListEntry(current_uuid_list_ptr,
                                                 smac_prefix_list_node,
                                                 mac_prefix_list_entry);

        free_mac_prefix_node(current_mac_prefix_list_node);

        mp_free( &geo_fence_config -> mac_prefix_list_node_mempool,
                current_mac_prefix_list_node);

    }

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_tracked_mac_list_node(ptracked_mac_list_node
                                                         tracked_mac_list_head){

    init_entry( &tracked_mac_list_head -> mac_list_entry);

    init_entry( &tracked_mac_list_head -> rssi_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_tracked_mac_list_node(ptracked_mac_list_node
                                                         tracked_mac_list_head){

    remove_list_node( &tracked_mac_list_head -> mac_list_entry);

    remove_list_node( &tracked_mac_list_head -> rssi_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_uuid_list_node(puuid_list_node uuid_list_node){

    memset(uuid_list_node -> uuid, 0, UUID_LENGTH);

    init_entry( &uuid_list_node -> uuid_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_uuid_list_node(puuid_list_node uuid_list_node){

    memset(uuid_list_node -> uuid, 0, UUID_LENGTH);

    remove_list_node( &uuid_list_node -> uuid_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_mac_prefix_node(pmac_prefix_list_node mac_prefix_node){

    memset(mac_prefix_node -> mac_prefix, 0, LENGTH_OF_MAC_ADDRESS);

    init_entry( &mac_prefix_node -> mac_prefix_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_mac_prefix_node(pmac_prefix_list_node mac_prefix_node){

    memset(mac_prefix_node -> mac_prefix, 0, LENGTH_OF_MAC_ADDRESS);

    remove_list_node( &mac_prefix_node -> mac_prefix_list_entry);

    return WORK_SUCCESSFULLY;

}
