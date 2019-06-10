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


ErrorCode init_geo_fence(pgeo_fence_config geo_fence_config)
{
#ifdef debugging
    printf("[GeoFence] Allicating Memory\n");
#endif

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

    geo_fence_config -> is_initialized = true;

    return WORK_SUCCESSFULLY;

}


ErrorCode release_geo_fence(pgeo_fence_config geo_fence_config)
{
    geo_fence_config -> is_initialized = false;

    pthread_mutex_destroy( &geo_fence_config -> geo_fence_list_lock);

    mp_destroy( &geo_fence_config -> tracked_mac_list_head_mempool);

    mp_destroy( &geo_fence_config -> rssi_list_node_mempool);

    mp_destroy( &geo_fence_config -> geo_fence_list_node_mempool);

    mp_destroy( &geo_fence_config -> uuid_list_node_mempool);

    mp_destroy( &geo_fence_config -> mac_prefix_list_node_mempool);

    mp_destroy( &geo_fence_config -> geo_fence_mac_prefix_list_node_mempool);

    return WORK_SUCCESSFULLY;
}


ErrorCode geo_fence_check_tracked_object_data_routine(
                                          pgeo_fence_config geo_fence_config, 
                                          int check_type, 
                                          BufferNode *buffer_node)
{

    /* lbeacon_uuid;lbeacon_datetime,lbeacon_ip;
     * object_type;object_number;object_mac_address_1;initial_timestamp_GMT_1;
     * final_timestamp_GMT_1;rssi_1;panic_button_1;object_type;object_number;
     * object_mac_address_2;initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2
     * ;panic_button_2;
     */

    pgeo_fence_list_node current_list_ptr;

    puuid_list_node current_uuid_list_ptr;

    List_Entry *current_list_entry, *current_uuid_list_entry;

    char *save_ptr, *uuid, *lbeacon_ip, *lbeacon_datetime, *topic_name;

    char content_temp[WIFI_MESSAGE_LENGTH];

    int return_value;

    BufferNode temp_buffer_node;

    if(geo_fence_config -> is_initialized == false){
        return E_API_INITIALLZATION;
    }

    current_list_ptr = NULL;
    save_ptr = NULL;
    topic_name = NULL;

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);

    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    topic_name = strtok_save( content_temp, DELIMITER_SEMICOLON, &save_ptr);

    uuid = strtok_save( NULL, DELIMITER_SEMICOLON, &save_ptr);

#ifdef debugging
    printf("[GeoFence] Current processing UUID: %s\n", uuid);
#endif

    lbeacon_datetime = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

    list_for_each(current_list_entry, &geo_fence_config ->
                  geo_fence_list_head.geo_fence_list_entry)
    {
        current_list_ptr = ListEntry(current_list_entry,
                                     sgeo_fence_list_node,
                                     geo_fence_list_entry);

        current_uuid_list_entry = NULL;

        switch(check_type)
        {
            case Fence:
                
                list_for_each(current_uuid_list_entry, &current_list_ptr ->
                              fence_uuid_list_head)
                {
                    current_uuid_list_ptr = ListEntry(current_uuid_list_entry,
                                                      suuid_list_node,
                                                      uuid_list_entry);

                    if(strncmp(uuid, current_uuid_list_ptr -> uuid, UUID_LENGTH)
                       == 0)
                    {
                        memset( &temp_buffer_node, 0, sizeof(BufferNode));

                        temp_buffer_node.pkt_direction = buffer_node -> 
                                                                pkt_direction;
                        temp_buffer_node.pkt_type = buffer_node -> pkt_type;
                        temp_buffer_node.port = buffer_node -> port;

                        sprintf(temp_buffer_node.content, 
                                "%d;%s;%d;%s", 
                                current_list_ptr -> id, 
                                "Fence",
                                current_uuid_list_ptr -> 
                                threshold,
                                buffer_node -> content);

#ifdef debugging
                        printf("[GeoFence] Content: %s\n", 
                                             temp_buffer_node.content);
#endif

                        temp_buffer_node.content_size = strlen(
                                             temp_buffer_node.content);

#ifdef debugging
                        printf("[GeoFence] Size: %d\n", 
                                       temp_buffer_node.content_size);
#endif

                        memcpy(temp_buffer_node.net_address, 
                               buffer_node-> net_address,
                               NETWORK_ADDR_LENGTH);

                        check_geo_fence_routine(geo_fence_config, 
                                                 &temp_buffer_node);
                    }
                
                    break;
                }
                break;

            case Perimeter:

                list_for_each(current_uuid_list_entry, &current_list_ptr ->
                              perimeters_uuid_list_head)
                {
                    current_uuid_list_ptr = ListEntry(current_uuid_list_entry,
                                                      suuid_list_node,
                                                      uuid_list_entry);

                    if(strncmp(uuid, current_uuid_list_ptr -> uuid, UUID_LENGTH)
                       ==  0)
                    {
                        memset( &temp_buffer_node, 0, sizeof(BufferNode));

                        temp_buffer_node.pkt_direction = buffer_node -> 
                                                                pkt_direction;
                        temp_buffer_node.pkt_type = buffer_node -> pkt_type;
                        temp_buffer_node.port = buffer_node -> port;

                        sprintf(temp_buffer_node.content, 
                                "%d;%s;%d;%s", 
                                current_list_ptr -> id, 
                                "Perimeter",
                                current_uuid_list_ptr -> 
                                threshold,
                                buffer_node -> content);

#ifdef debugging
                        printf("[GeoFence] Content: %s\n", 
                                             temp_buffer_node -> content);
#endif

                        temp_buffer_node.content_size = strlen(
                                             temp_buffer_node.content);

#ifdef debugging
                        printf("[GeoFence] Size: %d\n", 
                                       temp_buffer_node.content_size);
#endif

                        memcpy(temp_buffer_node.net_address, 
                               buffer_node-> net_address,
                               NETWORK_ADDR_LENGTH);

                        check_geo_fence_routine(geo_fence_config, 
                                                 &temp_buffer_node);
                    }
                    
                    break;
                }

                break;
        }

    }

    pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);

    return WORK_SUCCESSFULLY;

}


static ErrorCode check_geo_fence_routine(pgeo_fence_config geo_fence_config, 
                                         BufferNode *buffer_node)
{
    List_Entry *current_list_entry, *current_mac_list_entry;

    sgeo_fence_list_node *current_list_ptr, *geo_fence_list_ptr;

    smac_prefix_list_node *current_mac_prefix_list_ptr;

    char *current_ptr, *saved_ptr, *uuid, *lbeacon_ip, *lbeacon_type,
         *mac_address, *initial_timestamp, *final_timestamp, *panic_button;

    int geo_fence_id, object_type, number_of_objects, rssi, threshold;

    char content_for_processing[WIFI_MESSAGE_LENGTH];

    char content_for_alert[WIFI_MESSAGE_LENGTH];

    BufferNode *GeoFence_alert_buffer_node;

    if(geo_fence_config -> is_initialized == false){
        return E_API_INITIALLZATION;
    }

    /*
     * id;"P/F";threshold;tracking_data
     */

    memset(content_for_processing, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_for_processing, buffer_node -> content, buffer_node -> 
           content_size);
    geo_fence_list_ptr  = NULL;

    current_ptr = strtok_save(content_for_processing, DELIMITER_SEMICOLON,
                           &saved_ptr);

    sscanf(current_ptr, "%d", &geo_fence_id);

    list_for_each(current_list_entry, &geo_fence_config ->
                                      geo_fence_list_head.geo_fence_list_entry)
    {

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

    if(geo_fence_list_ptr == NULL)
    {

#ifdef debugging
        printf("[GeoFence] Exit\n");
#endif

        return WORK_SUCCESSFULLY;
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

        while(number_of_objects --)
        {
            //mac_address;initial_timestamp;final_timestamp;rssi;panic_button;

            mac_address = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            initial_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, 
                                             &saved_ptr);

            final_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON,
                                           &saved_ptr);

            current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            sscanf(current_ptr, "%d", &rssi);

            panic_button = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            list_for_each(current_mac_list_entry, &geo_fence_list_ptr ->
                                                          mac_prefix_list_head)
            {
                current_mac_prefix_list_ptr = ListEntry(current_mac_list_entry,
                                                        smac_prefix_list_node,
                                                        mac_prefix_list_entry);

                if((strncmp_caseinsensitive(mac_address,
                        current_mac_prefix_list_ptr -> mac_prefix,
                        strlen(current_mac_prefix_list_ptr -> mac_prefix)) == 0)
                        && rssi >= threshold)
                {

                    printf("[GeoFence-Alert] timestamp %d - %s %s %s %d\n",
                           get_system_time(), uuid, lbeacon_ip, mac_address,
                           rssi);

                    memset(content_for_alert, 0, WIFI_MESSAGE_LENGTH);

                    //number_of_geo_fence_alert;mac_address1;type1;uuid1;
                    //alert_time1;rssi1;geo_fence_id1;

                    GeoFence_alert_buffer_node = NULL;

                    while(GeoFence_alert_buffer_node == NULL)
                    {
                        GeoFence_alert_buffer_node = mp_alloc(geo_fence_config 
                                           -> GeoFence_alert_list_node_mempool);
                    }

                    memset(GeoFence_alert_buffer_node, 0, sizeof(BufferNode));

                    sprintf(GeoFence_alert_buffer_node -> content, 
                            "%d;%s;%s;%s;%s;%d;%d;", 1, mac_address, 
                            lbeacon_type, uuid, final_timestamp, rssi, 
                            geo_fence_id);

                    GeoFence_alert_buffer_node -> content_size = 
                                        strlen(GeoFence_alert_buffer_node -> 
                                        content) * sizeof(char);
                    
                    GeoFence_alert_buffer_node -> pkt_type = from_modules;
                    GeoFence_alert_buffer_node -> pkt_direction =       
                                                            GeoFence_alert_data;
                    
                    // GeoFence_alert_buffer_node -> net_address 
                    // GeoFence_alert_buffer_node -> port

                    pthread_mutex_lock( &geo_fence_config  -> 
                                         GeoFence_alert_list_head -> list_lock);

                    insert_list_tail( &GeoFence_alert_buffer_node -> 
                                     buffer_entry,
                                      &geo_fence_config -> 
                                     GeoFence_alert_list_head -> list_head);

                    pthread_mutex_unlock( &geo_fence_config  -> 
                                         GeoFence_alert_list_head -> list_lock);

                }
            }

        }

    }while(object_type != (max_type - 1));

    return WORK_SUCCESSFULLY;

}


ErrorCode update_geo_fence(pgeo_fence_config geo_fence_config, 
                           BufferNode* buffer_node)
{
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

    if(geo_fence_config -> is_initialized == false){
        return E_API_INITIALLZATION;
    }

    pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

    number_of_geo_fence = -1;
    current_ptr = NULL;
    saveptr = NULL;

    current_ptr = strtok_save(buffer_node -> content,
                              DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
    printf("[GeoFence] Topic: %s\n", current_ptr);
#endif

    current_ptr = strtok_save(NULL,
                              DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
    printf("[GeoFence] number_of_geo_fence: %s\n", current_ptr);
#endif

    sscanf(current_ptr, "%d", &number_of_geo_fence);

    for (counter = 0; counter < number_of_geo_fence; counter ++)
    {

        char *save_current_ptr, *uuid, *threshold_str;

        puuid_list_node uuid_list_node;
        pmac_prefix_list_node mac_prefix_node;
        pgeo_fence_list_node geo_fence_list_node;

        int number_of_perimeters, number_of_fence, 
            number_of_mac_prefix, threshold;

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
                                      geo_fence_list_head.geo_fence_list_entry)
        {

            current_list_ptr = ListEntry(current_list_entry,
                                         sgeo_fence_list_node,
                                         geo_fence_list_entry);

#ifdef debugging
            printf("[GeoFence] current_list_ptr->id: %d\n[GeoFence] "\
                   "geo_fence_id: %d\n", current_list_ptr->id, geo_fence_id);
#endif

            if(current_list_ptr->id == geo_fence_id)
            {

                geo_fence_list_node = current_list_ptr;

                break;
            }

        }

        if(geo_fence_list_node == NULL)
        {
            geo_fence_list_node = mp_alloc( &geo_fence_config ->
                                           geo_fence_list_node_mempool);

            if(geo_fence_list_node != NULL)
            {
                init_geo_fence_list_node(geo_fence_list_node);

#ifdef debugging
                printf("[GeoFence] Not Exists\n");
#endif
            }
        }
        else
        {
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
            counter_for_lbeacon ++)
        {

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
            if(uuid_list_node != NULL)
            {
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
            counter_for_lbeacon ++)
        {

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
            if(uuid_list_node != NULL)
            {
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
            number_of_mac_prefix;counter_for_mac_prefix ++)
        {

            current_ptr = NULL;
            mac_prefix_node = NULL;

            current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_current_ptr);

            mac_prefix_node = mp_alloc( &geo_fence_config ->
                                       mac_prefix_list_node_mempool);
            if(mac_prefix_node != NULL)
            {
                init_mac_prefix_node(mac_prefix_node);

                memcpy(mac_prefix_node -> mac_prefix, current_ptr,
                       strlen(current_ptr) * sizeof(char));
    #ifdef debugging
                printf("[GeoFence] MAC_Prefix: %s\n", mac_prefix_node -> 
                                                      mac_prefix);
    #endif
                insert_list_tail( &mac_prefix_node -> mac_prefix_list_entry,
                                  &geo_fence_list_node -> mac_prefix_list_head);

            }
        }

        insert_list_tail( &geo_fence_list_node -> geo_fence_list_entry,
                          &geo_fence_list_head -> geo_fence_list_entry);

    }

    pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);

    return WORK_SUCCESSFULLY;
}


static ErrorCode init_geo_fence_list_node(pgeo_fence_list_node
                                                           geo_fence_list_node)
{

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
                                      pgeo_fence_config geo_fence_config)
{

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
                        &geo_fence_list_node -> perimeters_uuid_list_head)
    {

        current_uuid_list_node = ListEntry(current_uuid_list_ptr,
                                           suuid_list_node,
                                           uuid_list_entry);

        free_uuid_list_node(current_uuid_list_node);

        mp_free( &geo_fence_config -> uuid_list_node_mempool,
                current_uuid_list_node);

    }

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node -> fence_uuid_list_head)
    {

        current_uuid_list_node = ListEntry(current_uuid_list_ptr,
                                           suuid_list_node,
                                           uuid_list_entry);

        free_uuid_list_node(current_uuid_list_node);

        mp_free( &geo_fence_config -> uuid_list_node_mempool,
                current_uuid_list_node);

    }

    list_for_each_safe(current_uuid_list_ptr, next_uuid_list_ptr,
                        &geo_fence_list_node -> mac_prefix_list_head)
    {

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
                                                         tracked_mac_list_head)
{

    init_entry( &tracked_mac_list_head -> mac_list_entry);

    init_entry( &tracked_mac_list_head -> rssi_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_tracked_mac_list_node(ptracked_mac_list_node
                                                         tracked_mac_list_head)
{

    remove_list_node( &tracked_mac_list_head -> mac_list_entry);

    remove_list_node( &tracked_mac_list_head -> rssi_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_uuid_list_node(puuid_list_node uuid_list_node)
{

    memset(uuid_list_node -> uuid, 0, UUID_LENGTH);

    init_entry( &uuid_list_node -> uuid_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_uuid_list_node(puuid_list_node uuid_list_node)
{

    memset(uuid_list_node -> uuid, 0, UUID_LENGTH);

    remove_list_node( &uuid_list_node -> uuid_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode init_mac_prefix_node(pmac_prefix_list_node mac_prefix_node)
{

    memset(mac_prefix_node -> mac_prefix, 0, LENGTH_OF_MAC_ADDRESS);

    init_entry( &mac_prefix_node -> mac_prefix_list_entry);

    return WORK_SUCCESSFULLY;

}


static ErrorCode free_mac_prefix_node(pmac_prefix_list_node mac_prefix_node)
{

    memset(mac_prefix_node -> mac_prefix, 0, LENGTH_OF_MAC_ADDRESS);

    remove_list_node( &mac_prefix_node -> mac_prefix_list_entry);

    return WORK_SUCCESSFULLY;

}
