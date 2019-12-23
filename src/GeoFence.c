/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     GeoFence.c

  File Description:

     This file provides APIs of geo-fence related functions to BOT server. 

  Version:

     1.0, 20191118

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

     Chun-Yu Lai   , chunyu1202@gmail.com
 */

#include "GeoFence.h"
#include "SqlWrapper.h"

/*
  construct_geo_fence_list:
*/

ErrorCode construct_geo_fence_list(char * database_argument, 
                                   GeoFenceListHead* geo_fence_list_head){

    void *db = NULL;
    FILE *file_geo_fence = NULL;
    char setting_buf[CONFIG_BUFFER_SIZE];
    char *save_ptr = NULL;
    char *area_id = NULL;
    char *id = NULL;
    char *name = NULL;
    char *perimeters = NULL;
    char *fences = NULL;

    List_Entry * current_area_list_entry = NULL;
    GeoFenceAreaNode *current_area_list_ptr = NULL;
    bool is_found_area_id = false;
    GeoFenceAreaNode *geo_fence_area_node = NULL;

    GeoFenceSettingNode *geo_fence_setting_node = NULL;

    char *save_ptr_per_setting = NULL;
    char *number_beacons = NULL;
    int number = 0;
    char *beacon_uuid = NULL;
    char *rssi = NULL;
    int retry_times = 0;

    
    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){
 
        zlog_error(category_debug, 
                   "cannot open database"); 
        return E_SQL_OPEN_DATABASE;
    }

    SQL_dump_active_geo_fence_settings(db, DUMP_ACTIVE_GEO_FENCE_FILE);

    SQL_close_database_connection(db);

    file_geo_fence = fopen(DUMP_ACTIVE_GEO_FENCE_FILE, "r");
    if(file_geo_fence == NULL){
        zlog_error(category_debug, "cannot open filepath %s", 
                   DUMP_ACTIVE_GEO_FENCE_FILE);
        return E_OPEN_FILE;
    }

    pthread_mutex_lock(&(geo_fence_list_head->list_lock));

    // Re-load geo-fence setting
    while(fgets(setting_buf, sizeof(setting_buf), file_geo_fence) != NULL){
            
        area_id = strtok_save(setting_buf, DELIMITER_SEMICOLON, &save_ptr);
        id = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
        name = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
        perimeters = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
        fences = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
        
        is_found_area_id = false;

        // Iterate the list to check whether the geo-fence area node with 
        // area_id already exists in the list
        list_for_each(current_area_list_entry, 
                      &geo_fence_list_head->list_head){

            current_area_list_ptr = ListEntry(current_area_list_entry,
                                              GeoFenceAreaNode,
                                              geo_fence_area_list_entry);  
            if(current_area_list_ptr->area_id == atoi(area_id)){
                is_found_area_id = true;
                break;
            }
        }

        // Create one new geo-fence area node with area_id if it does not 
        // exist in the list.
        if(is_found_area_id == false){
            retry_times = MEMORY_ALLOCATE_RETRIES;
            while(retry_times --){
                geo_fence_area_node = mp_alloc(&geofence_area_mempool);
                if(NULL != geo_fence_area_node)
                    break;
            }
            if(NULL == geo_fence_area_node){
                zlog_error(category_debug, 
                           "construct_geo_fence_list (geo_fence_area_node) " \
                           "mp_alloc failed, abort this data");
                continue;
            }

            memset(geo_fence_area_node, 0, sizeof(GeoFenceAreaNode));

            geo_fence_area_node->area_id = atoi(area_id);
            init_entry(&geo_fence_area_node->geo_fence_area_list_entry);
            init_entry(&geo_fence_area_node->geo_fence_setting_list_head);

            insert_list_tail(&geo_fence_area_node -> geo_fence_area_list_entry,
                             &geo_fence_list_head -> list_head);
        }
        // Iterate the list again to locate the geo-fence area node with area_id
        list_for_each(current_area_list_entry, &geo_fence_list_head->list_head){

            current_area_list_ptr = ListEntry(current_area_list_entry,
                                              GeoFenceAreaNode,
                                              geo_fence_area_list_entry);  

            if(current_area_list_ptr->area_id == atoi(area_id)){
                // Create one new geo-fence setting node with id into the 
                // geo-fence area node
                retry_times = MEMORY_ALLOCATE_RETRIES;
                while(retry_times --){
                    geo_fence_setting_node = mp_alloc(&geofence_setting_mempool);
                    if(NULL != geo_fence_setting_node)
                        break;
                }
                if(NULL == geo_fence_setting_node){
                    zlog_error(category_debug, 
                               "construct_geo_fence_list (geo_fence_setting_node) " \
                               "mp_alloc failed, abort this data");
                    continue;
                }

                memset(geo_fence_setting_node, 0, sizeof(GeoFenceSettingNode));

                init_entry(&geo_fence_setting_node->
                           geo_fence_setting_list_entry);

                geo_fence_setting_node->id = atoi(id);
                strcpy(geo_fence_setting_node->name, name);
                strcpy(geo_fence_setting_node->perimeters_lbeacon_setting, 
                       perimeters);            

                number_beacons = strtok(perimeters, 
                                        DELIMITER_COMMA, 
                                        &save_ptr_per_setting);          
                number = atoi(number_beacons);
                while(number--){
                    beacon_uuid = strtok(NULL, 
                                         DELIMITER_COMMA, 
                                         &save_ptr_per_setting);
                }
                rssi = strtok(NULL, DELIMITER_COLON, &save_ptr_per_setting);
                
                geo_fence_setting_node->rssi_of_perimeters = atoi(rssi);
                
                strcpy(geo_fence_setting_node->fences_lbeacon_setting, fences);

                number_beacons = strtok(fences, 
                                        DELIMITER_COMMA, 
                                        &save_ptr_per_setting);

                number = atoi(number_beacons);

                while(number--){
                    beacon_uuid = strtok(NULL, 
                                         DELIMITER_COMMA, 
                                         &save_ptr_per_setting);
                }
                rssi = strtok(NULL, DELIMITER_COLON, &save_ptr_per_setting);

                geo_fence_setting_node->rssi_of_fences = atoi(rssi);

                insert_list_tail(&geo_fence_setting_node -> 
                                 geo_fence_setting_list_entry,
                                 &current_area_list_ptr ->
                                 geo_fence_setting_list_head);
                
                break;
            }
        }

    }
    fclose(file_geo_fence);

    pthread_mutex_unlock(&(geo_fence_list_head->list_lock));

    //remove(DUMP_ACTIVE_GEO_FENCE_FILE);
    //remove(DUMP_GEO_FENCE_OBJECTS_FILE);

    return WORK_SUCCESSFULLY;
}

/*
  destroy_geo_fence_list:
*/

ErrorCode destroy_geo_fence_list(GeoFenceListHead* geo_fence_list_head){

    List_Entry * current_area_list_entry = NULL;
    List_Entry * next_area_list_entry = NULL;
    GeoFenceAreaNode * current_area_list_ptr = NULL;
    List_Entry * current_setting_list_entry = NULL;
    List_Entry * next_setting_list_entry = NULL;
    GeoFenceSettingNode * current_setting_list_ptr = NULL;


    pthread_mutex_lock(&(geo_fence_list_head->list_lock));

    list_for_each_safe(current_area_list_entry, 
                       next_area_list_entry, 
                       &geo_fence_list_head->list_head){
        
        remove_list_node(current_area_list_entry);

        current_area_list_ptr = ListEntry(current_area_list_entry,
                                          GeoFenceAreaNode,
                                          geo_fence_area_list_entry);  

        list_for_each_safe(current_setting_list_entry,
                           next_setting_list_entry,
                           &current_area_list_ptr -> 
                           geo_fence_setting_list_head){

            remove_list_node(current_setting_list_entry);

            current_setting_list_ptr = 
                ListEntry(current_setting_list_entry,
                          GeoFenceSettingNode,
                          geo_fence_setting_list_entry);
         
            mp_free(&geofence_setting_mempool, current_setting_list_ptr);
        }
         
        mp_free(&geofence_area_mempool, current_area_list_ptr);
    }

    pthread_mutex_unlock(&(geo_fence_list_head->list_lock));

    return WORK_SUCCESSFULLY;
}

ErrorCode construct_objects_list_under_geo_fence_monitoring(
    char * database_argument,
    ObjectWithGeoFenceListHead* objects_list_head){

    void *db = NULL;
    List_Entry * current_objects_in_area_list_entry = NULL;
    ObjectWithGeoFenceAreaNode *current_objects_in_area_list_ptr = NULL;
    
    FILE *file_objects = NULL;
    char object_buf[CONFIG_BUFFER_SIZE];
    char *save_ptr = NULL;
    char *area_id = NULL;
    char *mac_address = NULL;
    bool is_found_area_id = false;

    ObjectWithGeoFenceAreaNode * object_with_geo_fence_area_node = NULL;

    int retry_times = 0;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){
 
        zlog_error(category_debug, 
                   "cannot open database"); 
        return E_SQL_OPEN_DATABASE;
    }

    SQL_dump_mac_address_under_geo_fence_monitor(db, 
                                                 DUMP_GEO_FENCE_OBJECTS_FILE);

    SQL_close_database_connection(db);

    file_objects = fopen(DUMP_GEO_FENCE_OBJECTS_FILE, "r");
    if(file_objects == NULL){
        zlog_error(category_debug, "cannot open filepath %s", 
                   DUMP_GEO_FENCE_OBJECTS_FILE);

        return E_OPEN_FILE;
    }

    pthread_mutex_lock(&(objects_list_head->list_lock));
    
    while(fgets(object_buf, sizeof(object_buf), file_objects) != NULL){
    
        area_id = strtok_save(object_buf, DELIMITER_SEMICOLON, &save_ptr);
        mac_address = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
      
        is_found_area_id = false;    
        // Re-load objects under geo-fence monitoring
        list_for_each(current_objects_in_area_list_entry, 
                      &objects_list_head->list_head){

            current_objects_in_area_list_ptr = 
                ListEntry(current_objects_in_area_list_entry,
                          ObjectWithGeoFenceAreaNode,
                          objects_area_list_entry);  
            
            if(current_objects_in_area_list_ptr->area_id == atoi(area_id)){
                is_found_area_id = true;

                strcat(current_objects_in_area_list_ptr ->
                       mac_address_under_monitor, 
                       mac_address);
                strcat(current_objects_in_area_list_ptr ->
                       mac_address_under_monitor, 
                       DELIMITER_SEMICOLON);

                break;
            }
        }
        
        if(is_found_area_id == false){
            
            retry_times = MEMORY_ALLOCATE_RETRIES;
            while(retry_times --){
                object_with_geo_fence_area_node = 
                    mp_alloc(&geofence_objects_area_mempool);

                if(NULL != object_with_geo_fence_area_node)
                    break;
            }
            if(NULL == object_with_geo_fence_area_node){
                zlog_error(category_debug, 
                           "construct_objects_list_under_geo_fence_monitoring " \
                           "(object_with_geo_fence_area_node) mp_alloc " \
                           "failed, abort this data");
                continue;
            }

            memset(object_with_geo_fence_area_node, 0, 
                   sizeof(ObjectWithGeoFenceAreaNode));

            object_with_geo_fence_area_node->area_id = atoi(area_id);
            init_entry(&object_with_geo_fence_area_node -> 
                       objects_area_list_entry);

            strcat(object_with_geo_fence_area_node->mac_address_under_monitor, 
                   mac_address);

            strcat(object_with_geo_fence_area_node->mac_address_under_monitor, 
                   DELIMITER_SEMICOLON);

            insert_list_tail(&object_with_geo_fence_area_node -> 
                             objects_area_list_entry,
                             &objects_list_head->list_head);   
        }   
    }

    fclose(file_objects);
    pthread_mutex_unlock(&(objects_list_head->list_lock));

    return WORK_SUCCESSFULLY;
}

ErrorCode destroy_objects_list_under_geo_fence_monitoring(
    ObjectWithGeoFenceListHead* objects_list_head){

    List_Entry * current_objects_in_area_list_entry = NULL;
    List_Entry * next_objects_in_area_list_entry = NULL;
    ObjectWithGeoFenceAreaNode * current_objects_in_area_list_ptr = NULL;

    pthread_mutex_lock(&(objects_list_head->list_lock));

    list_for_each_safe(current_objects_in_area_list_entry, 
                       next_objects_in_area_list_entry, 
                       &objects_list_head->list_head){
        
        remove_list_node(current_objects_in_area_list_entry);

        current_objects_in_area_list_ptr = 
            ListEntry(current_objects_in_area_list_entry,
                      ObjectWithGeoFenceAreaNode,
                      objects_area_list_entry);  
         
        mp_free(&geofence_objects_area_mempool, 
                current_objects_in_area_list_ptr);
    }

    pthread_mutex_unlock(&(objects_list_head->list_lock));

    return WORK_SUCCESSFULLY;
}

ErrorCode check_geo_fence_violations(
    BufferNode *buffer_node,  
    char * database_argument,
    GeoFenceListHead* geo_fence_list_head,
    ObjectWithGeoFenceListHead * objects_list_head,
    GeoFenceViolationListHead * geo_fence_violation_list_head,
    int perimeter_valid_duration_in_sec)
{
     /* The format of the tracked object data:
        lbeacon_uuid;lbeacon_datetime,lbeacon_ip;object_type;object_number;
        object_mac_address_1;initial_timestamp_GMT_1;final_timestamp_GMT_1;
        rssi_1;panic_button_1;battery_voltage_1;object_type;object_number;
        object_mac_address_2;initial_timestamp_GMT_2;final_timestamp_GMT_2;
        rssi_2;panic_button_2;battery_voltage_2;
     */
    char content_temp[WIFI_MESSAGE_LENGTH];
    char *save_ptr = NULL;

    List_Entry * current_area_list_entry = NULL;
    GeoFenceAreaNode *current_area_list_ptr = NULL;

    char *lbeacon_uuid = NULL;
    char lbeacon_area[LENGTH_OF_UUID]; 
    const int FIRST_N_CHARACTERS_FOR_AREA_ID = 4;
    int area_id = 0;

    List_Entry * current_setting_list_entry = NULL;
    GeoFenceSettingNode *current_setting_list_ptr = NULL;

    bool is_lbeacon_part_of_perimeter = false;
    bool is_lbeacon_part_of_fence = false;

   
    zlog_info(category_debug, ">>check_geo_fence_violations");
  
    memset(content_temp, 0, sizeof(content_temp));
    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    lbeacon_uuid = strtok_save(content_temp, DELIMITER_SEMICOLON, &save_ptr);
    memset(lbeacon_area, 0, sizeof(lbeacon_area));
    strncpy(lbeacon_area, lbeacon_uuid, FIRST_N_CHARACTERS_FOR_AREA_ID);
    area_id = atoi(lbeacon_area);

    pthread_mutex_lock(&(geo_fence_list_head->list_lock));

    list_for_each(current_area_list_entry, &geo_fence_list_head->list_head){

        current_area_list_ptr = ListEntry(current_area_list_entry,
                                          GeoFenceAreaNode,
                                          geo_fence_area_list_entry);

        if(current_area_list_ptr->area_id == area_id){
           list_for_each(current_setting_list_entry, 
                          &current_area_list_ptr -> 
                          geo_fence_setting_list_head){
            
                current_setting_list_ptr = 
                    ListEntry(current_setting_list_entry,
                              GeoFenceSettingNode,
                              geo_fence_setting_list_entry);

                if(strstr(current_setting_list_ptr -> 
                          perimeters_lbeacon_setting, 
                          lbeacon_uuid) != NULL){
 
                    examine_object_tracking_data(
                        buffer_node, 
                        area_id,
                        LBEACON_PERIMETER,   
                        current_setting_list_ptr->rssi_of_perimeters,
                        objects_list_head,
                        geo_fence_violation_list_head,
                        perimeter_valid_duration_in_sec,
                        database_argument);                        
                }

                if(strstr(current_setting_list_ptr -> 
                          fences_lbeacon_setting, 
                          lbeacon_uuid) != NULL){
                    
                    examine_object_tracking_data(
                        buffer_node, 
                        area_id,
                        LBEACON_FENCE,                               
                        current_setting_list_ptr->rssi_of_perimeters,
                        objects_list_head,
                        geo_fence_violation_list_head,
                        perimeter_valid_duration_in_sec,
                        database_argument);                
                }
            }            
        }        
    }

    pthread_mutex_unlock(&(geo_fence_list_head->list_lock));

    zlog_info(category_debug, "<<check_geo_fence_violations");


    return WORK_SUCCESSFULLY;
}

ErrorCode examine_object_tracking_data(
    BufferNode *buffer_node,
    int area_id,
    LBeaconType lbeacon_type,
    int rssi_criteria,
    ObjectWithGeoFenceListHead * objects_list_head,
    GeoFenceViolationListHead * geo_fence_violation_list_head,
    int perimeter_valid_duration_in_sec,
    char * database_argument)
{

     /* The format of the tracked object data:
        lbeacon_uuid;lbeacon_datetime,lbeacon_ip;object_type;object_number;
        object_mac_address_1;initial_timestamp_GMT_1;final_timestamp_GMT_1;
        rssi_1;panic_button_1;battery_voltage_1;object_type;object_number;
        object_mac_address_2;initial_timestamp_GMT_2;final_timestamp_GMT_2;
        rssi_2;panic_button_2;battery_voltage_2;
     */
    char content_temp[WIFI_MESSAGE_LENGTH];
    char *save_ptr = NULL;

    char *lbeacon_uuid = NULL;
    char *lbeacon_datetime = NULL;
    char *lbeacon_ip = NULL;
    const int MAX_OBJECT_TYPES = 2; // BLE and BR_EDR
    int types = 0;
    char *object_type = NULL;
    char *object_number = NULL;
    int number = 0;
    char *object_mac_address = NULL;
    char *initial_timestamp_GMT = NULL;
    char *final_timestamp_GMT = NULL;
    char *rssi = NULL;
    int scanned_rssi = 0;
    char *panic_button = NULL;
    char *battery_voltage = NULL;

    List_Entry * current_objects_in_area_list_entry = NULL;
    ObjectWithGeoFenceAreaNode *current_objects_in_area_list_ptr = NULL;

    char mac_address[LENGTH_OF_MAC_ADDRESS];
    char mac_address_in_lower_case[LENGTH_OF_MAC_ADDRESS];

    List_Entry * current_violation_list_entry = NULL;
    List_Entry * next_violation_list_entry = NULL;
    GeoFenceViolationNode *current_violation_list_ptr = NULL;

    bool is_found_mac_address = false;
    int current_time = get_system_time();

    GeoFenceViolationNode *new_node = NULL;
    void *db = NULL;
    int retry_times = 0;

    
    zlog_info(category_debug, ">>examine_object_tracking_data");
  
    memset(content_temp, 0, sizeof(content_temp));
    memcpy(content_temp, 
           buffer_node -> content, 
           buffer_node -> content_size);

    lbeacon_uuid = strtok_save(content_temp, 
                               DELIMITER_SEMICOLON, 
                               &save_ptr);
    lbeacon_datetime = strtok_save(NULL, 
                                   DELIMITER_SEMICOLON, 
                                   &save_ptr);
    lbeacon_ip = strtok_save(NULL, 
                             DELIMITER_SEMICOLON, 
                             &save_ptr);

    types = MAX_OBJECT_TYPES;

    while(types--){
        
        object_type = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
        object_number = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

        number = atoi(object_number);

        while(number--){
        
            object_mac_address = strtok_save(NULL, 
                                             DELIMITER_SEMICOLON, 
                                             &save_ptr);
            initial_timestamp_GMT = strtok_save(NULL, 
                                                DELIMITER_SEMICOLON, 
                                                &save_ptr);
            final_timestamp_GMT = strtok_save(NULL, 
                                              DELIMITER_SEMICOLON, 
                                              &save_ptr);
            rssi = strtok_save(NULL, 
                               DELIMITER_SEMICOLON, 
                               &save_ptr);
            scanned_rssi = atoi(rssi);

            panic_button = strtok_save(NULL, 
                                       DELIMITER_SEMICOLON, 
                                       &save_ptr);
            battery_voltage = strtok_save(NULL, 
                                          DELIMITER_SEMICOLON, 
                                          &save_ptr);

            memset(mac_address, 0, sizeof(mac_address));
            strcpy(mac_address, object_mac_address);

            memset(mac_address_in_lower_case, 0, 
                   sizeof(mac_address_in_lower_case));

            if(WORK_SUCCESSFULLY != 
               strtolowercase(mac_address, 
                              mac_address_in_lower_case, 
                              sizeof(mac_address_in_lower_case))){
                continue;
            } 

            
            // Check if mac_address_in_lower_case is part of objects under 
            // geo-fence monitoring
            pthread_mutex_lock(&(objects_list_head->list_lock));

            list_for_each(current_objects_in_area_list_entry, 
                          &objects_list_head->list_head){

                current_objects_in_area_list_ptr = 
                    ListEntry(current_objects_in_area_list_entry,
                              ObjectWithGeoFenceAreaNode,
                              objects_area_list_entry);

                if(current_objects_in_area_list_ptr->area_id == area_id){
                  
                    if(NULL == strstr(current_objects_in_area_list_ptr -> 
                                      mac_address_under_monitor, 
                                      mac_address_in_lower_case)){
                        
                        continue;
                    }
                   
                    if(scanned_rssi < rssi_criteria){
                        continue;
                    }
                    
                    // mac_address violates geo-fence settings with lbeacon_type
                    pthread_mutex_lock(&(geo_fence_violation_list_head -> 
                                         list_lock));

                    list_for_each_safe(current_violation_list_entry, 
                                       next_violation_list_entry,
                                       &geo_fence_violation_list_head -> 
                                       list_head){

                        current_violation_list_ptr = 
                            ListEntry(current_violation_list_entry,
                                      GeoFenceViolationNode, 
                                      geo_fence_violation_list_entry);

                        if(strncmp(current_violation_list_ptr->mac_address, 
                                   mac_address_in_lower_case, 
                                   strlen(mac_address_in_lower_case)) == 0){

                            is_found_mac_address = true;

                            if(lbeacon_type == LBEACON_FENCE && 
                               perimeter_valid_duration_in_sec > 
                               current_time - 
                               current_violation_list_ptr -> 
                               perimeter_violation_timestamp){

                                zlog_info(category_debug, 
                                          "fence violation: " \
                                          "mac_address=[%s], " \
                                          "area_id=[%d]",
                                          mac_address_in_lower_case,
                                          area_id);
                                
                                remove_list_node(current_violation_list_entry);
                                mp_free(&geofence_violation_mempool, 
                                        current_violation_list_ptr);

                                if(WORK_SUCCESSFULLY != 
                                   SQL_open_database_connection(
                                       database_argument, 
                                       &db)){
 
                                    zlog_error(category_debug, 
                                               "cannot open database"); 
                                    continue;
                                }

                                SQL_identify_geofence_violation(
                                    db, 
                                    mac_address_in_lower_case);

                                SQL_close_database_connection(db);

                                break;

                            }else if(lbeacon_type == LBEACON_PERIMETER){

                                current_violation_list_ptr -> 
                                    perimeter_violation_timestamp = current_time;

                                zlog_info(category_debug, 
                                          "perimeter violation: " \
                                          "mac_address=[%s], " \
                                          "area_id=[%d]",
                                          mac_address_in_lower_case,
                                          area_id);
                            }
                            break;
                        }else if(current_violation_list_ptr -> 
                                 perimeter_violation_timestamp < 
                                 current_time - perimeter_valid_duration_in_sec){
                        
                            remove_list_node(current_violation_list_entry);
                            mp_free(&geofence_violation_mempool, 
                                    current_violation_list_ptr);
                        }
                    }  

                    // only create new node in violation list while the newly 
                    // violations is LBEACON_PERIMETER. The reason is that
                    // fence violations without previous valid perimter 
                    // violations should be ignored.
                    if(lbeacon_type == LBEACON_PERIMETER && 
                       is_found_mac_address == false){
                        
                        retry_times = MEMORY_ALLOCATE_RETRIES;
                        while(retry_times --){
                            new_node = mp_alloc(&geofence_violation_mempool);
                            if(NULL != new_node)
                                break;
                        }
                        if(NULL == new_node){
                            zlog_error(category_debug, 
                                       "examine_object_tracking_data " \
                                       "(new_node) mp_alloc " \
                                       "failed, abort this data");
                            continue;
                        }
                        memset(new_node, 0, sizeof(GeoFenceViolationNode));

                        init_entry(&new_node->geo_fence_violation_list_entry);

                        strcpy(new_node->mac_address, 
                               mac_address_in_lower_case);
                    
                        new_node->perimeter_violation_timestamp = 
                            current_time;

                        insert_list_tail(&new_node -> 
                                         geo_fence_violation_list_entry, 
                                         &geo_fence_violation_list_head -> 
                                         list_head);
                    }

                    pthread_mutex_unlock(&(geo_fence_violation_list_head -> 
                                         list_lock));

                    break;
                }
            }
            pthread_mutex_unlock(&(objects_list_head->list_lock));
        }    
    }

    zlog_info(category_debug, "<<examine_object_tracking_data");

    return WORK_SUCCESSFULLY;
}


