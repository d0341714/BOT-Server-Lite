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

     1.0, 20190617

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

    /* Initialize the mempool for the buffer node using in the GeoFence */
    if(mp_init( &(geo_fence_config -> mempool), 
               (sizeof(sgeo_fence_list_node) * 2), 
               SLOTS_IN_MEM_POOL) != MEMORY_POOL_SUCCESS)
        return E_MALLOC;

    init_entry( &(geo_fence_config -> geo_fence_list_head.geo_fence_list_entry));

#ifdef debugging
    printf("[GeoFence] Allicating Memory success\n");
#endif

    pthread_mutex_init(&geo_fence_config -> geo_fence_list_lock, 0);

    return WORK_SUCCESSFULLY;
}


ErrorCode release_geo_fence(pgeo_fence_config geo_fence_config)
{
    pthread_mutex_destroy( &geo_fence_config -> geo_fence_list_lock);

    mp_destroy( &geo_fence_config -> mempool);

    return WORK_SUCCESSFULLY;
}


ErrorCode geo_fence_check_tracked_object_data_routine(
                                          pgeo_fence_config geo_fence_config, 
                                          BufferNode *buffer_node)
{
    pgeo_fence_list_node current_list_ptr;

    List_Entry *current_list_entry, *current_uuid_list_entry;

    char *save_ptr, *uuid, *lbeacon_ip, *lbeacon_datetime, 
         *save_current_ptr, *current_ptr, *geo_fence_uuid,
         *threshold_str;

    char content_temp[WIFI_MESSAGE_LENGTH];

    char fence[WIFI_MESSAGE_LENGTH];
    char perimeter[WIFI_MESSAGE_LENGTH];

    int return_value, threshold, counter_for_lbeacon, 
        number_of_perimeters, number_of_fence;

    BufferNode temp_buffer_node;

    current_list_ptr = NULL;
    current_list_entry = NULL;
    current_uuid_list_entry = NULL;
    save_ptr = NULL;
    uuid = NULL;
    lbeacon_ip = NULL;
    lbeacon_datetime = NULL;
    save_current_ptr = NULL;
    current_ptr = NULL;
    geo_fence_uuid = NULL;
    threshold_str = NULL;
    return_value = 0;
    threshold = 0;
    counter_for_lbeacon = 0;
    number_of_perimeters = 0;
    number_of_fence = 0;

    /* The format of the tracked object data:
     * lbeacon_uuid;lbeacon_datetime,lbeacon_ip;object_type;object_number;
     * object_mac_address_1;initial_timestamp_GMT_1;final_timestamp_GMT_1;
     * rssi_1;panic_button_1;object_type;object_number;object_mac_address_2;
     * initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2;panic_button_2;
     */

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);

    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    uuid = strtok_save( content_temp, DELIMITER_SEMICOLON, &save_ptr);

#ifdef debugging
    printf("[GeoFence] Current processing UUID: %s\n", uuid);
#endif

    lbeacon_datetime = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

    /* Find the UUID in every geo fence */
    list_for_each(current_list_entry, &geo_fence_config ->
                  geo_fence_list_head.geo_fence_list_entry)
    {
        current_list_ptr = ListEntry(current_list_entry,
                                     sgeo_fence_list_node,
                                     geo_fence_list_entry);

        memset(fence, 0, WIFI_MESSAGE_LENGTH);
        memcpy(fence, current_list_ptr->fences, 
               strlen(current_list_ptr->fences) * sizeof(char));
        
        memset(perimeter, 0, WIFI_MESSAGE_LENGTH);
        memcpy(perimeter, current_list_ptr->perimeters, 
               strlen(current_list_ptr->perimeters) * sizeof(char));

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_save(fence, DELIMITER_COMMA, &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_fence);

#ifdef debugging
        printf("[GeoFence] number_of_fence: %d\n", number_of_fence);
#endif

        /* Go through the fence list */
        for(counter_for_lbeacon = 0;counter_for_lbeacon < number_of_fence;
            counter_for_lbeacon ++)
        {
            geo_fence_uuid = NULL;
            threshold_str = NULL;

            geo_fence_uuid = strtok_save(NULL, DELIMITER_COMMA,
                                          &save_current_ptr);

#ifdef debugging
            printf("[GeoFence] UUID: %s\n", geo_fence_uuid);
#endif
            threshold_str = strtok_save(NULL, DELIMITER_COMMA,
                                         &save_current_ptr);

            sscanf(threshold_str, "%d", &threshold);

#ifdef debugging
            printf("[GeoFence] Threshold: %d\n", threshold);
#endif
            if(strncmp(geo_fence_uuid, uuid, UUID_LENGTH) == 0)
            {
                memset( &temp_buffer_node, 0, sizeof(BufferNode));

                temp_buffer_node.pkt_direction = buffer_node -> pkt_direction;
                temp_buffer_node.pkt_type = buffer_node -> pkt_type;
                temp_buffer_node.port = buffer_node -> port;

                sprintf(temp_buffer_node.content, "%d;%s;%d;%s;%s", 
                                                  current_list_ptr -> id, 
                                                  "Fence",
                                                  threshold,
                                                  current_list_ptr -> 
                                                  mac_prefix,
                                                  buffer_node -> content);

#ifdef debugging
                printf("[GeoFence] Content: %s\n", temp_buffer_node.content);
#endif

                temp_buffer_node.content_size = strlen(
                                                      temp_buffer_node.content);

#ifdef debugging
                printf("[GeoFence] Size: %d\n", temp_buffer_node.content_size);
#endif

                memcpy(temp_buffer_node.net_address, buffer_node-> net_address,
                       NETWORK_ADDR_LENGTH);

                check_geo_fence_routine(geo_fence_config, current_list_ptr,
                                         &temp_buffer_node);

                break;
            }
        }

        current_ptr = NULL;
        save_current_ptr = NULL;

        current_ptr = strtok_save(perimeter, DELIMITER_COMMA,
                                   &save_current_ptr);
        sscanf(current_ptr, "%d", &number_of_perimeters);

#ifdef debugging
        printf("[GeoFence] number_of_perimeters: %d\n", number_of_perimeters);
#endif

        /* Go through the perimeter list */
        for(counter_for_lbeacon = 0;counter_for_lbeacon < number_of_perimeters;
            counter_for_lbeacon ++)
        {
            geo_fence_uuid = NULL;
            threshold_str = NULL;

            geo_fence_uuid = strtok_save(NULL, DELIMITER_COMMA,
                                          &save_current_ptr);

#ifdef debugging
            printf("[GeoFence] UUID: %s\n", uuid);
#endif
            threshold_str = strtok_save(NULL, DELIMITER_COMMA,
                                         &save_current_ptr);

            sscanf(threshold_str, "%d", &threshold);

#ifdef debugging
            printf("[GeoFence] Threshold: %d\n", threshold);
#endif
            if(strncmp(geo_fence_uuid, uuid, UUID_LENGTH) == 0)
            {
                
                memset( &temp_buffer_node, 0, sizeof(BufferNode));

                temp_buffer_node.pkt_direction = buffer_node -> pkt_direction;
                temp_buffer_node.pkt_type = buffer_node -> pkt_type;
                temp_buffer_node.port = buffer_node -> port;

                sprintf(temp_buffer_node.content, "%d;%s;%d;%s;%s", 
                                                  current_list_ptr -> id, 
                                                  "Perimeter",
                                                  threshold,
                                                  current_list_ptr -> 
                                                  mac_prefix,
                                                  buffer_node -> content);

#ifdef debugging
                printf("[GeoFence] Content: %s\n", temp_buffer_node.content);
#endif

                temp_buffer_node.content_size = strlen(
                                                      temp_buffer_node.content);

#ifdef debugging
                printf("[GeoFence] Size: %d\n", temp_buffer_node.content_size);
#endif

                memcpy(temp_buffer_node.net_address, buffer_node-> net_address,
                       NETWORK_ADDR_LENGTH);

                check_geo_fence_routine(geo_fence_config, current_list_ptr, 
                                         &temp_buffer_node);

                break;
            }
        }
    }

    pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);

    return WORK_SUCCESSFULLY;
}


static ErrorCode check_geo_fence_routine(pgeo_fence_config geo_fence_config,
                                         pgeo_fence_list_node 
                                         geo_fence_list_ptr,
                                         BufferNode *buffer_node)
{
    List_Entry *current_list_entry, *current_mac_list_entry;

    sgeo_fence_list_node *current_list_ptr;

    char *current_ptr, *saved_ptr, *uuid, *lbeacon_ip, *lbeacon_type,
         *mac_address, *initial_timestamp, *final_timestamp, *panic_button, 
         *mac_prefix, *current_mac_prefix, *mac_prefix_save_ptr;

    int geo_fence_id, object_type, number_of_objects, rssi, threshold,
        number_of_mac_prefix;

    char content_for_processing[WIFI_MESSAGE_LENGTH];

    char content_for_alert[WIFI_MESSAGE_LENGTH];

    char content_for_mac_prefix[WIFI_MESSAGE_LENGTH];

    BufferNode *GeoFence_alert_buffer_node;

    current_list_entry = NULL;
    current_mac_list_entry = NULL;

    current_list_ptr = NULL;

    current_ptr = NULL;
    saved_ptr = NULL;
    uuid = NULL;
    lbeacon_ip = NULL;
    lbeacon_type = NULL;
    mac_address = NULL;
    initial_timestamp = NULL;
    final_timestamp = NULL;
    panic_button = NULL;
    mac_prefix = NULL;
    current_mac_prefix = NULL;
    mac_prefix_save_ptr = NULL;
    GeoFence_alert_buffer_node = NULL;
    geo_fence_id = 0;
    object_type = 0;
    number_of_objects = 0;
    rssi = 0;
    threshold = 0;
    number_of_mac_prefix = 0;

    /* The format of the content in the buffer_node:
     * id;"P/F";threshold;mac_prefix;tracking_data
     */

    memset(content_for_processing, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_for_processing, buffer_node -> content, buffer_node -> 
           content_size);

    current_ptr = strtok_save(content_for_processing, DELIMITER_SEMICOLON,
                           &saved_ptr);

    sscanf(current_ptr, "%d", &geo_fence_id);

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
    
    mac_prefix = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

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
            /* The part of the format of the tracked object data:
             * mac_address;initial_timestamp;final_timestamp;rssi;panic_button; 
             */

            mac_address = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            initial_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, 
                                             &saved_ptr);

            final_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON,
                                           &saved_ptr);

            current_ptr = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            sscanf(current_ptr, "%d", &rssi);

            panic_button = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

            current_mac_prefix = NULL;
            mac_prefix_save_ptr = NULL;

            memset(content_for_mac_prefix, 0, WIFI_MESSAGE_LENGTH);
            memcpy(content_for_mac_prefix, mac_prefix,
                   strlen(mac_prefix) * sizeof(char));

            current_mac_prefix = strtok_save(content_for_mac_prefix, 
                                             DELIMITER_COMMA,
                                              &mac_prefix_save_ptr);

            sscanf(current_mac_prefix, "%d", &number_of_mac_prefix);

            while(number_of_mac_prefix --)
            {
                current_mac_prefix = strtok_save(NULL, DELIMITER_COMMA,
                                                  &mac_prefix_save_ptr);

                if((strncmp_caseinsensitive(mac_address, current_mac_prefix, 
                                            strlen(current_mac_prefix)) == 0) &&
                                            rssi >= threshold)
                {

                    printf("[GeoFence-Alert] timestamp %d - %s %s %s %d\n",
                           get_system_time(), uuid, lbeacon_ip, mac_address,
                           rssi);

                    memset(content_for_alert, 0, WIFI_MESSAGE_LENGTH);

                    /* The format of the GeoFence alert:
                     * number_of_geo_fence_alert;mac_address1;type1;uuid1;
                     * alert_time1;rssi1;geo_fence_id1;
                     */

                    GeoFence_alert_buffer_node = NULL;

                    while(GeoFence_alert_buffer_node == NULL)
                    {
                        GeoFence_alert_buffer_node = mp_alloc(
                                            geo_fence_config ->   
                                            GeoFence_alert_list_node_mempool);
                        Sleep(WAITING_TIME);
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
                                     buffer_entry, &geo_fence_config -> 
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

    List_Entry *current_list_entry;

    int number_of_geo_fence, geo_fence_id, counter;

    char *name, *saveptr, *current_ptr, *fences, *perimeters, *mac_prefix;

    geo_fence_list_node = NULL;
    current_list_ptr = NULL;

    current_list_entry = NULL;

    number_of_geo_fence = 0;
    geo_fence_id = 0;
    counter = 0;
    name = NULL;
    saveptr = NULL;
    current_ptr = NULL;
    fences = NULL;
    perimeters = NULL;
    mac_prefix = NULL;

    current_ptr = strtok_save(buffer_node -> content,
                              DELIMITER_SEMICOLON, &saveptr);

    sscanf(current_ptr, "%d", &number_of_geo_fence);

#ifdef debugging
    printf("[GeoFence] number_of_geo_fence: %d\n", number_of_geo_fence);
#endif

    pthread_mutex_lock( &geo_fence_config -> geo_fence_list_lock);

    for (counter = 0; counter < number_of_geo_fence; counter ++)
    {
        char *save_current_ptr;

        current_ptr = NULL;
        name = NULL;
        geo_fence_list_node = NULL;

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

        list_for_each(current_list_entry, 
                  &geo_fence_config -> geo_fence_list_head.geo_fence_list_entry)
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

        if(geo_fence_list_node != NULL)
        {
            free_geo_fence_list_node(geo_fence_list_node);
            mp_free( &geo_fence_config -> mempool, geo_fence_list_node);

#ifdef debugging
            printf("[GeoFence] Exists\n");
#endif
        }
       
        geo_fence_list_node = NULL;

        while(geo_fence_list_node == NULL)
        {
            geo_fence_list_node = mp_alloc( &geo_fence_config -> mempool);
            Sleep(WAITING_TIME);
        }

        init_geo_fence_list_node(geo_fence_list_node);

        if(sizeof(saveptr) > 0)
        {
            perimeters = strtok_save(NULL, DELIMITER_SEMICOLON, 
                                     &saveptr);

#ifdef debugging
            printf("[GeoFence] Perimeters: [%s]\n", perimeters);
#endif

            fences = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
            printf("[GeoFence] Fences: [%s]\n", fences);
#endif

            mac_prefix = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

#ifdef debugging
            printf("[GeoFence] Mac_Prefix: [%s]\n", mac_prefix);
#endif
        
            geo_fence_list_node->id = geo_fence_id;

            memcpy(geo_fence_list_node->name, name, strlen(name) * 
                   sizeof(char));

            memset(geo_fence_list_node -> fences, 0, WIFI_MESSAGE_LENGTH);
            memset(geo_fence_list_node -> perimeters, 0, WIFI_MESSAGE_LENGTH);
            memset(geo_fence_list_node -> mac_prefix, 0, WIFI_MESSAGE_LENGTH);

            memcpy(geo_fence_list_node -> fences, fences, 
                   strlen(fences) * sizeof(char));
            memcpy(geo_fence_list_node -> perimeters, perimeters, 
                   strlen(perimeters) * sizeof(char));
            memcpy(geo_fence_list_node -> mac_prefix, mac_prefix, 
                   strlen(mac_prefix) * sizeof(char));

            insert_list_tail( &geo_fence_list_node -> geo_fence_list_entry,
                              &geo_fence_list_head -> geo_fence_list_entry);
        }
    }
    pthread_mutex_unlock( &geo_fence_config -> geo_fence_list_lock);

    return WORK_SUCCESSFULLY;
}


static ErrorCode init_geo_fence_list_node(
                                       pgeo_fence_list_node geo_fence_list_node)
{
    geo_fence_list_node -> id = -1;

    memset(geo_fence_list_node -> name, 0, WIFI_MESSAGE_LENGTH);

    init_entry( &geo_fence_list_node -> geo_fence_list_entry);

    return WORK_SUCCESSFULLY;
}


static ErrorCode free_geo_fence_list_node(
                                       pgeo_fence_list_node geo_fence_list_node)
{
    remove_list_node( &geo_fence_list_node -> geo_fence_list_entry);

    geo_fence_list_node -> id = -1;

    memset(geo_fence_list_node -> name, 0, WIFI_MESSAGE_LENGTH);
    memset(geo_fence_list_node ->fences, 0, WIFI_MESSAGE_LENGTH);
    memset(geo_fence_list_node ->perimeters, 0, WIFI_MESSAGE_LENGTH);
    memset(geo_fence_list_node ->mac_prefix, 0, WIFI_MESSAGE_LENGTH);

    return WORK_SUCCESSFULLY;
}