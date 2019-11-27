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


ErrorCode add_geo_fence_settings(Memory_Pool *geofence_mempool,
                                 struct List_Entry *geo_fence_list_head, 
                                 char *buf,
                                 char *database_argument){

    char *current_ptr = NULL;
    char *save_ptr = NULL;

    /* Each geo-fence is defined in server.conf as following format:

       geofence_1=前門;0001;0,24;1,00010018000000006660000000011910,-53,; \
       1,00010018000000006660000000011900,-53,;
       
       The active period of this geo-fence is defined in the "0,24" 
       setting, and we call it "hours_duration" here. The hours_duration
       setting is in the format of "hour_start,hour_end" to specify the 
       active starting hour and ending hour.
    */
    char *name = NULL;
    char *hours_duration = NULL;
    char *perimeters = NULL;
    char *fences = NULL;
    char *hour_start = NULL;
    char *hour_end = NULL;
    void *db = NULL;
    char perimeters_config[CONFIG_BUFFER_SIZE];
    char fences_config[CONFIG_BUFFER_SIZE];

    GeoFenceListNode *new_node = NULL;

    int i = 0;
    char *temp_value = NULL;
    int retry_times;


    memset(perimeters_config, 0, sizeof(perimeters_config));
    memset(fences_config, 0, sizeof(fences_config));

    zlog_info(category_debug, ">> add_geo_fence_settings");
    zlog_info(category_debug, "GeoFence data=[%s]", buf);

    name = strtok_save(buf, DELIMITER_SEMICOLON, &save_ptr);

    hours_duration = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    perimeters = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
    strcpy(perimeters_config, perimeters);
    
    fences = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
    strcpy(fences_config, fences);

    zlog_info(category_debug, 
              "name=[%s], perimeters=[%s], fences=[%s]", 
              name, perimeters, fences);

    retry_times = MEMORY_ALLOCATE_RETRY;
    while(retry_times --){
        new_node = mp_alloc(geofence_mempool);

        if(NULL != new_node)
            break;
    }
           
    if(NULL == new_node){
        zlog_error(category_health_report,
                   "[add_geo_fence_settings] mp_alloc failed, abort this data");

        return E_MALLOC;
    }

    memset(new_node, 0, sizeof(GeoFenceListNode));

    init_entry(&new_node -> geo_fence_list_entry);
                    
    memcpy(new_node->name, name, strlen(name));

    // parse hours duration
    hour_start = strtok_save(hours_duration, DELIMITER_COMMA, &save_ptr);
    hour_end = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);

    // parse perimeters settings
    current_ptr = strtok_save(perimeters, DELIMITER_COMMA, &save_ptr);
    new_node->number_LBeacons_in_perimeter = atoi (current_ptr);
    if(new_node->number_LBeacons_in_perimeter > 
       MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER)
    {
        zlog_error(category_debug,
                   "number_perimeters[%d] exceeds our maximum support number[%d]",
                   new_node->number_LBeacons_in_perimeter,
                   MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER);
    }
    else
    {
        if(new_node->number_LBeacons_in_perimeter > 0){
            for(i = 0 ; i < new_node->number_LBeacons_in_perimeter ; i++){
                temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
                strcpy(new_node->perimeters[i], temp_value);
            }
            current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
            new_node->rssi_of_perimeters = atoi(current_ptr);
        }
    }

    // parse fences settings
    current_ptr = strtok_save(fences, DELIMITER_COMMA, &save_ptr);
    new_node->number_LBeacons_in_fence = atoi (current_ptr);
    if(new_node->number_LBeacons_in_fence > 
       MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE)
    {
        zlog_error(category_debug,
                   "number_fences[%d] exceeds our maximum support number[%d]",
                   new_node->number_LBeacons_in_fence,
                   MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE);
    }
    else
    {
        if(new_node->number_LBeacons_in_fence > 0){
            for(i = 0 ; i < new_node->number_LBeacons_in_fence ; i++){
                temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
                strcpy(new_node->fences[i], temp_value);
            }
            current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
            new_node->rssi_of_fences = atoi(current_ptr);
        }
    }
    
    insert_list_tail( &new_node -> geo_fence_list_entry,
                      geo_fence_list_head);

    zlog_info(category_debug, "<<add_geo_fence_settings");

    return WORK_SUCCESSFULLY;
}


ErrorCode check_geo_fence_violations(BufferNode *buffer_node,  
                                     struct List_Entry * list_head, 
                                     int perimeter_valid_duration_in_sec, 
                                     int granularity_for_continuous_violation_in_sec,
                                     char * database_argument)
{
     /* The format of the tracked object data:
        lbeacon_uuid;lbeacon_datetime,lbeacon_ip;object_type;object_number;
        object_mac_address_1;initial_timestamp_GMT_1;final_timestamp_GMT_1;
        rssi_1;panic_button_1;object_type;object_number;object_mac_address_2;
        initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2;panic_button_2;
     */
    char content_temp[WIFI_MESSAGE_LENGTH];
    char *save_ptr = NULL;

    List_Entry * current_list_entry = NULL;
    GeoFenceListNode *current_list_ptr = NULL;

    char *lbeacon_uuid = NULL;
    
    int i = 0;

    bool is_perimeter_lbeacon = false;
    bool is_fence_lbeacon = false;

    void *db = NULL;


    zlog_info(category_debug, ">>check_geo_fence_violations");

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    lbeacon_uuid = strtok_save(content_temp, DELIMITER_SEMICOLON, &save_ptr);
    

    list_for_each(current_list_entry, list_head){

        current_list_ptr = ListEntry(current_list_entry,
                                     GeoFenceListNode,
                                     geo_fence_list_entry);

        if(current_list_ptr->is_active == 0){
            continue;
        }

        // Check if lbeacon_uuid is listed as perimeter LBeacon or 
        // fence LBeacon of this geo-fence setting
        is_perimeter_lbeacon = false;
        is_fence_lbeacon = false;

        for(i = 0 ; i < current_list_ptr->number_LBeacons_in_perimeter ; i++){
            if(0 == strncmp_caseinsensitive(lbeacon_uuid, 
                                            current_list_ptr->perimeters[i], 
                                            strlen(lbeacon_uuid))){
                is_perimeter_lbeacon = true;
                break;
            }
        }

        for(i = 0 ; i < current_list_ptr->number_LBeacons_in_fence ; i++){
            if(0 == strncmp_caseinsensitive(lbeacon_uuid, 
                                            current_list_ptr->fences[i], 
                                            strlen(lbeacon_uuid))){
                is_fence_lbeacon = true;
                break;
            }
        }

        // If lbeacon_uuid is not part of this geo-fence setting, continue 
        // to check next geo-fence setting
        if(is_perimeter_lbeacon == false && is_fence_lbeacon == false){

           zlog_info(category_debug, 
                     "lbeacon_uuid=[%s] is not in geo-fence setting " \
                     "name=[%s]", 
                     lbeacon_uuid, 
                     current_list_ptr->name);

            continue;
        }
        
        // start to examine object status
        memset(content_temp, 0, sizeof(content_temp));
        memcpy(content_temp, 
               buffer_node -> content, 
               buffer_node -> content_size);

        examine_tracked_objects_status(buffer_node->API_version,
                                       content_temp, 
                                       is_fence_lbeacon,
                                       current_list_ptr->rssi_of_fences,
                                       is_perimeter_lbeacon, 
                                       current_list_ptr->rssi_of_perimeters,
                                       perimeter_valid_duration_in_sec, 
                                       granularity_for_continuous_violation_in_sec,
                                       database_argument);      
    }


    zlog_info(category_debug, "<<check_geo_fence_violations");


    return WORK_SUCCESSFULLY;
}



ErrorCode examine_tracked_objects_status(float api_version,
                                         char *buf,
                                         bool is_fence_lbeacon,
                                         int fence_rssi,
                                         bool is_perimeter_lbeacon,
                                         int perimeter_rssi,
                                         int perimeter_valid_duration_in_sec, 
                                         int granularity_for_continuous_violation_in_sec,
                                         char *database_argument)
{
    /* The format of the tracked object data:
        lbeacon_uuid;lbeacon_datetime,lbeacon_ip;object_type;object_number;
        object_mac_address_1;initial_timestamp_GMT_1;final_timestamp_GMT_1;
        rssi_1;panic_button_1;object_type;object_number;object_mac_address_2;
        initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2;panic_button_2;
     */

    char *save_ptr = NULL;
    char *lbeacon_uuid = NULL;
    char *lbeacon_datetime = NULL;
    char *lbeacon_ip = NULL;
    /* We set number_types as 2, because we need to parse BLE and 
       BR_EDR types */
    int number_types = 2;
    char *object_type = NULL;
    char *string_number_objects = NULL;
    int number_objects = 0;
    char *mac_address = NULL;
    char *initial_timestamp = NULL;
    char *final_timestamp = NULL;
    char *rssi = NULL;
    int detected_rssi = 0;
    char *panic_button = NULL;
    char *battery_voltage = NULL;
    void *db = NULL;
    ObjectMonitorType object_monitor_type = MONITOR_NORMAL;
    int is_valid_perimeter;


    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){
 
        zlog_error(category_debug, 
                   "cannot open database"); 
        return E_SQL_OPEN_DATABASE;
    }

    lbeacon_uuid = strtok_save(buf, 
                               DELIMITER_SEMICOLON, 
                               &save_ptr);
    lbeacon_datetime = strtok_save(NULL, 
                                   DELIMITER_SEMICOLON, 
                                   &save_ptr);

    lbeacon_ip = strtok_save(NULL, 
                             DELIMITER_SEMICOLON, 
                             &save_ptr);

    while(number_types --){
        object_type = strtok_save(NULL, 
                                  DELIMITER_SEMICOLON, 
                                  &save_ptr);
        
        string_number_objects = strtok_save(NULL, 
                                            DELIMITER_SEMICOLON, 
                                            &save_ptr);
        if(string_number_objects == NULL){
            SQL_close_database_connection(db);
            return E_API_PROTOCOL_FORMAT;
        }
        number_objects = atoi(string_number_objects);

        while(number_objects--){
            mac_address = strtok_save(NULL, 
                                      DELIMITER_SEMICOLON, 
                                      &save_ptr);

            initial_timestamp = strtok_save(NULL, 
                                            DELIMITER_SEMICOLON, 
                                            &save_ptr);

            final_timestamp = strtok_save(NULL, 
                                          DELIMITER_SEMICOLON, 
                                          &save_ptr);

            rssi = strtok_save(NULL, 
                               DELIMITER_SEMICOLON, 
                               &save_ptr);
            if(rssi == NULL){
                SQL_close_database_connection(db);
                return E_API_PROTOCOL_FORMAT;
            }
            detected_rssi = atoi(rssi);

            panic_button = strtok_save(NULL, 
                                       DELIMITER_SEMICOLON, 
                                       &save_ptr);

            if(atof(BOT_SERVER_API_VERSION_20) == api_version){
                /* We do not have additional fields in 
                   BOT_SERVER_API_VERSION_20, the all fields are already 
                   listed above */           
            }else{
                battery_voltage = strtok_save(NULL, 
                                              DELIMITER_SEMICOLON, 
                                              &save_ptr);
            }

            // get object type

            /* check fence and perimeter */
            if(is_fence_lbeacon && ( detected_rssi > fence_rssi)){

                zlog_info(category_debug, 
                          "[GeoFence-Fence]: LBeacon UUID=[%s] "\
                          "mac_address=[%s]", 
                          lbeacon_uuid, mac_address);


                //check if user violated perimeter first
                is_valid_perimeter = 0;

                SQL_check_perimeter_violation_valid(
                    db, 
                    mac_address, 
                    &is_valid_perimeter);
                
                 zlog_info(category_debug, 
                              "[GeoFence-Fence]: is_valid_perimter=[%d]",
                              is_valid_perimeter);

                if(is_valid_perimeter){

                    SQL_insert_geofence_violation_event(
                        db,
                        mac_address,
                        lbeacon_uuid,
                        granularity_for_continuous_violation_in_sec);

                    SQL_identify_geofence_violation(
                        db,
                        mac_address,
                        lbeacon_uuid,
                        detected_rssi);
                }
            }
            if(is_perimeter_lbeacon && (detected_rssi > perimeter_rssi)){

                zlog_info(category_debug, 
                          "[GeoFence-Perimeter]: LBeacon UUID=[%s]" \
                          "mac_address=[%s]", 
                          lbeacon_uuid, mac_address);

                SQL_insert_geofence_perimeter_valid_deadline(
                    db,
                    mac_address,
                   perimeter_valid_duration_in_sec);
            }
        }
    }
    
    SQL_close_database_connection(db);

    return WORK_SUCCESSFULLY;
}