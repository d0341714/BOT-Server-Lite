/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Server.c

  File Description:

     This file contains programs to transmit and receive data to and from
     Gateway through Wi-Fi network, and programs executed by network setup and
     initialization, Beacon health monitor and comminication unit.

  Version:

     1.0, 20190726

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

     Holly Wang     , hollywang@iis.sinica.edu.tw
     Jake Lee       , jakelee@iis.sinica.edu.tw
     Ray Chao       , raychao5566@gmail.com
     Gary Xiao      , garyh0205@hotmail.com
     Chun-Yu Lai    , chunyu1202@gmail.com
 */


#include "Server.h"

int main(int argc, char **argv)
{
    int return_value;

    /* The type of the packet sent by the server */
    int send_pkt_type;

    /* The command message to be sent */
    char command_msg[WIFI_MESSAGE_LENGTH];

    int uptime;

    /* Number of character bytes in the packet content */
    int content_size;

    /* The main thread of the communication Unit */
    pthread_t CommUnit_thread;

    /* The thread of maintenance database */
    pthread_t database_maintenance_thread;

    /* The thread of summarizing location information */
    pthread_t location_information_thread;

    /* The thread of collecting violation events */
    pthread_t collect_violation_thread;

    /* The thread to listen for messages from Wi-Fi interface */
    pthread_t wifi_listener_thread;

    BufferNode *current_node;

    char content[WIFI_MESSAGE_LENGTH];

    /* Initialize flags */
    NSI_initialization_complete      = false;
    CommUnit_initialization_complete = false;
    initialization_failed            = false;
    ready_to_work                    = true;


    /* Initialize zlog */
    if(zlog_init(ZLOG_CONFIG_FILE_NAME) == 0)
    {
        category_health_report = zlog_get_category(LOG_CATEGORY_HEALTH_REPORT);

        if (!category_health_report)
            zlog_fini();

        category_debug = zlog_get_category(LOG_CATEGORY_DEBUG);
        if (!category_debug)
            zlog_fini();
    }

    zlog_info(category_debug,"Start Server");

    zlog_info(category_debug,"Mempool Initializing");

    /* Initialize the memory pool for buffer nodes */
    if(mp_init( &node_mempool, sizeof(BufferNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS)
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool for geo-fence structs */
    if(mp_init( &geofence_mempool, sizeof(GeoFenceListNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS)
    {
        return E_MALLOC;
    }

    zlog_info(category_debug,"Mempool Initialized");

    /* Create the config from input serverconfig file */

    if(get_server_config( &config, &common_config, CONFIG_FILE_NAME) 
       != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "Opening config file Fail");
        zlog_error(category_debug, "Opening config file Fail");
        return E_OPEN_FILE;
    }

    zlog_info(category_debug,"Initialize buffer lists");

    /* Initialize the address map*/
    init_Address_Map( &Gateway_address_map);

    /* Initialize buffer_list_heads and add to the head in to the priority list.
     */
    init_buffer( &priority_list_head, (void *) sort_priority_list,
                common_config.high_priority);

    init_buffer( &Geo_fence_alert_buffer_list_head,
                (void *) process_GeoFence_alert_routine, 
                common_config.time_critical_priority);
    insert_list_tail( &Geo_fence_alert_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &Geo_fence_receive_buffer_list_head,
                (void *) process_tracked_data_from_geofence_gateway, 
                common_config.time_critical_priority);
    insert_list_tail( &Geo_fence_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &LBeacon_receive_buffer_list_head,
                (void *) Server_LBeacon_routine, 
                common_config.normal_priority);
    insert_list_tail( &LBeacon_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_send_buffer_list_head,
                (void *) Server_process_wifi_send, 
                common_config.normal_priority);
    insert_list_tail( &NSI_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_receive_buffer_list_head,
                (void *) Server_NSI_routine, 
                common_config.normal_priority);
    insert_list_tail( &NSI_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_receive_buffer_list_head,
                (void *) Server_BHM_routine, 
                common_config.low_priority);
    insert_list_tail( &BHM_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_send_buffer_list_head,
                (void *) Server_process_wifi_send, 
                common_config.low_priority);
    insert_list_tail( &BHM_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    sort_priority_list(&common_config, &priority_list_head);

    zlog_info(category_debug,"Buffer lists initialize");

    zlog_info(category_debug,"Initialize sockets");

    /* Initialize the Wifi connection */
    if(udp_initial( &udp_config, config.recv_port) != WORK_SUCCESSFULLY){

        /* Error handling and return */
        initialization_failed = true;
        zlog_error(category_health_report, "Fail to initialize sockets");
        zlog_error(category_debug, "Fail to initialize sockets");

        return E_WIFI_INIT_FAIL;
    }

    return_value = startThread( &wifi_listener_thread, 
                               (void *)Server_process_wifi_receive,
                               NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        initialization_failed = true;
        return E_WIFI_INIT_FAIL;
    }
   zlog_info(category_debug,"Sockets initialized");

    NSI_initialization_complete = true;

    zlog_info(category_debug,"Network Setup and Initialize success");

    zlog_info(category_debug,"Initialize Communication Unit");

    /* Create the main thread of Communication Unit  */
    return_value = startThread( &CommUnit_thread, CommUnit_routine, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "CommUnit_thread Create Fail");
        zlog_error(category_debug, "CommUnit_thread Create Fail");
        return return_value;
    }

    /* Create thread to main database */
    return_value = startThread( &database_maintenance_thread, 
                                maintain_database, 
                                NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "Maintain database fail");
        zlog_error(category_debug, "Maintain database fail");
        return return_value;
    }

    /* Create thread to summarize location information */
    return_value = startThread( &location_information_thread, 
                                Server_summarize_location_information, 
                                NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, 
                   "Server_summarize_location_information fail");
        zlog_error(category_debug, 
                   "Server_summarize_location_information fail");
        return return_value;
    }

    /* Create thread to collect notification events */
    return_value = startThread( &collect_violation_thread, 
                                Server_collect_violation_event, 
                                NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, 
                   "Server_collect_violation_event fail");
        zlog_error(category_debug, 
                   "Server_collect_violation_event fail");
        return return_value;
    }

    zlog_info(category_debug,"Start Communication");

    /* The while loop waiting for CommUnit routine to be ready */
    while(CommUnit_initialization_complete == false)
    {
        sleep_t(BUSY_WAITING_TIME_IN_MS);

        if(initialization_failed == true)
        {
            ready_to_work = false;

            /* Release the Wifi elements and close the connection. */
            udp_release( &udp_config);

            return E_INITIALIZATION_FAIL;
        }
    }

    last_polling_object_tracking_time = 0;
    last_polling_LBeacon_for_HR_time = 0;

    /* The while loop that keeps the program running */
    while(ready_to_work == true)
    {
        uptime = get_clock_time();

        /* Time: period_between_RFTOD 7 < period_between_RFHR 3600 */

        /* If it is the time to poll track object data from LBeacons, 
           get a thread to do this work */
        if(uptime - last_polling_object_tracking_time >=
           config.period_between_RFTOD)
        {
            /* Poll object tracking object data */
            /* set the pkt type */

            memset(command_msg, 0, WIFI_MESSAGE_LENGTH);
            sprintf(command_msg, "%d;%d;%s;", from_server, 
                                              tracked_object_data, 
                                              BOT_SERVER_API_VERSION_LATEST);

#ifdef debugging
            display_time();
#endif
            zlog_info(category_debug,"Send Request for Tracked Object Data");

            /* Broadcast poll messenge to gateways */
            Broadcast_to_gateway(&Gateway_address_map, command_msg,
                                 WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_object_tracking_time */
            last_polling_object_tracking_time = uptime;
        }

        /* Since period_between_RFTOD is short, we only allow one type 
           of data to be sent at a time except for tracked object data. 
         */
        if(uptime - last_polling_LBeacon_for_HR_time >=
                config.period_between_RFHR)
        {
            /* Polling for health reports. */
            memset(command_msg, 0, WIFI_MESSAGE_LENGTH);
            sprintf(command_msg, "%d;%d;%s;", from_server, 
                                              gateway_health_report, 
                                              BOT_SERVER_API_VERSION_LATEST);

#ifdef debugging
            display_time();
#endif
            zlog_info(category_debug,"Send Request for Health Report");

            /* broadcast to gateways */
            Broadcast_to_gateway(&Gateway_address_map, command_msg,
                                 WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_LBeacon_for_HR_time */
            last_polling_LBeacon_for_HR_time = uptime;
        }
        else
        {
            sleep_t(BUSY_WAITING_TIME_IN_MS);
        }
    }/* End while(ready_to_work == true) */

    /* Release the Wifi elements and close the connection. */
    udp_release( &udp_config);

    mp_destroy(&node_mempool);

    mp_destroy(&geofence_mempool);

    return WORK_SUCCESSFULLY;
}


ErrorCode get_server_config(ServerConfig *config, 
                            CommonConfig *common_config, 
                            char *file_name) 
{
    FILE *file = fopen(file_name, "r");
    char config_message[CONFIG_BUFFER_SIZE];
    int number_geofence_settings = 0;
    int i = 0;

    List_Entry *current_list_entry = NULL;
    GeoFenceListNode *current_list_ptr = NULL;

    if (file == NULL) 
    {
        zlog_error(category_health_report, "Load serverconfig fail");
        zlog_info(category_debug, "Load serverconfig fail");
        return E_OPEN_FILE;
    }
    
    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->server_ip, config_message, sizeof(config->server_ip));
    zlog_info(category_debug,"Server IP [%s]", config->server_ip);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->db_ip, config_message, sizeof(config->db_ip));
    zlog_info(category_debug,"Database IP [%s]", config->db_ip);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->allowed_number_nodes = atoi(config_message);
    zlog_info(category_debug,"Allow Number of Nodes [%d]", 
              config->allowed_number_nodes);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->period_between_RFHR = atoi(config_message);
    zlog_info(category_debug,
              "Periods between request for health report " \
              "period_between_RFHR [%d]",
              config->period_between_RFHR);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->period_between_RFTOD = atoi(config_message);
    zlog_info(category_debug,
              "Periods between request for tracked object data " \
              "period_between_RFTOD [%d]",
              config->period_between_RFTOD);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->period_between_check_object_location = atoi(config_message);
    zlog_info(category_debug,
              "period_between_check_object_location [%d]",
              config->period_between_check_object_location);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->period_between_check_object_activity = atoi(config_message);
    zlog_info(category_debug,
              "period_between_check_object_activity [%d]",
              config->period_between_check_object_activity);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->number_worker_threads = atoi(config_message);
    zlog_info(category_debug,
              "Number of worker threads [%d]",
              common_config->number_worker_threads);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->send_port = atoi(config_message);
    zlog_info(category_debug,
              "The destination port when sending [%d]", 
              config->send_port);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->recv_port = atoi(config_message);
    zlog_info(category_debug,
              "The received port [%d]", config->recv_port);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->database_port = atoi(config_message);
    zlog_info(category_debug, 
              "The database port [%d]", config->database_port);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->database_name, config_message, 
           sizeof(config->database_name));
    zlog_info(category_debug,
              "Database Name [%s]", config->database_name);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->database_account, config_message, \
           sizeof(config->database_account));
    zlog_info(category_debug,
              "Database Account [%s]", config->database_account);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->database_password, config_message, 
           sizeof(config->database_password));
    
    memset(database_argument, 0, SQL_TEMP_BUFFER_LENGTH);

    sprintf(database_argument, "dbname=%s user=%s password=%s host=%s port=%d",
            config->database_name, 
            config->database_account,
            config->database_password, 
            config->db_ip,
            config->database_port );

    memset(config->database_password, 0, sizeof(config->database_password));

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->database_keep_days = atoi(config_message);
    zlog_info(category_debug,
              "Database database_keep_days [%d]", 
              config->database_keep_days);
        
    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->time_critical_priority = atoi(config_message);
    zlog_info(category_debug,
              "The nice of time critical priority is [%d]",
              common_config->time_critical_priority);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->high_priority = atoi(config_message);
    zlog_info(category_debug,
              "The nice of high priority is [%d]", 
              common_config->high_priority);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->normal_priority = atoi(config_message);
    zlog_info(category_debug,
              "The nice of normal priority is [%d]",
              common_config->normal_priority);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->low_priority = atoi(config_message);
    zlog_info(category_debug,
              "The nice of low priority is [%d]", 
              common_config->low_priority);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->location_time_interval_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "The location_time_interval_in_sec is [%d]", 
              config->location_time_interval_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_panic_button_monitor = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_panic_button_monitor is [%d]", 
              config->is_enabled_panic_button_monitor);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->panic_time_interval_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "The panic_time_interval_in_sec is [%d]", 
              config->panic_time_interval_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_geofence_monitor = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_geofence_monitor is [%d]", 
              config->is_enabled_geofence_monitor);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->geofence_monitor_config.geo_fence_time_interval_in_sec = 
        atoi(config_message);
    zlog_info(category_debug,
              "The geo_fence_time_interval_in_sec is [%d]", 
              config->geofence_monitor_config.
              geo_fence_time_interval_in_sec);

        zlog_info(category_debug, "Initialize geo-fence list");

    /* Initialize geo-fence list head to store all the geo-fence settings */
    init_entry( &(config->geo_fence_list_head));

    fetch_next_string(file, config_message, sizeof(config_message)); 
    number_geofence_settings = atoi(config_message);

    for(i = 0; i < number_geofence_settings ; i++){
          
        fetch_next_string(file, config_message, sizeof(config_message)); 
        add_geo_fence_settings(&(config->geo_fence_list_head), config_message);
    }

#ifdef debugging
    list_for_each(current_list_entry, &(config->geo_fence_list_head)){
        current_list_ptr = ListEntry(current_list_entry,
                                     GeoFenceListNode,
                                     geo_fence_list_entry);
            
        zlog_info(category_debug, "unique_key=[%s], name=[%s]", 
                  current_list_ptr->unique_key,
                  current_list_ptr->name);

        zlog_info(category_debug, "[perimeters] count=%d", 
                  current_list_ptr->number_perimeters);
        for(i = 0 ; i < current_list_ptr->number_perimeters ; i++)
            zlog_info(category_debug, "perimeter=[%s], rssi=[%d]", 
                      current_list_ptr->perimeters[i], 
                      current_list_ptr->rssi_of_perimeters);

        zlog_info(category_debug, "[fences] count=%d", 
                  current_list_ptr->number_fences);
        for(i = 0 ; i < current_list_ptr->number_fences ; i++)
            zlog_info(category_debug, "fence=[%s], rssi=[%d]", 
                      current_list_ptr->fences[i], 
                      current_list_ptr->rssi_of_fences);

        zlog_info(category_debug,"[mac_prefixes] count=%d", 
                  current_list_ptr->number_mac_prefixes);
        for(i = 0 ; i < current_list_ptr->number_mac_prefixes ; i++)
            zlog_info(category_debug,"mac_prefix=[%s]", 
                      current_list_ptr->mac_prefixes[i]);
   }
#endif
       
    zlog_info(category_debug, "geo-fence list initialized");

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_movement_monitor = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_movement_monitor is [%d]", 
              config->is_enabled_movement_monitor);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->movement_monitor_config.time_interval_in_min = atoi(config_message);
    zlog_info(category_debug,
              "The time_interval_in_min is [%d]", 
              config->movement_monitor_config.
              time_interval_in_min);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->movement_monitor_config.each_time_slot_in_min = atoi(config_message);
    zlog_info(category_debug,
              "The each_time_slot_in_min is [%d]", 
              config->movement_monitor_config.
              each_time_slot_in_min);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->movement_monitor_config.rssi_delta = atoi(config_message);
    zlog_info(category_debug,
              "The rssi_delta is [%d]", 
              config->movement_monitor_config.
              rssi_delta);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_collect_violation_event = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_collect_violation_event is [%d]", 
              config->is_enabled_collect_violation_event);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->collect_violation_event_time_interval_in_sec = 
        atoi(config_message);
    zlog_info(category_debug,
              "The collect_violation_event_time_interval_in_sec is [%d]", 
              config->collect_violation_event_time_interval_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->granularity_for_continuous_violations_in_sec = 
        atoi(config_message);
    zlog_info(category_debug,
              "The granularity_for_continuous_violations_in_sec " \
              "is [%d]", 
              config->granularity_for_continuous_violations_in_sec);

    fclose(file);

    return WORK_SUCCESSFULLY;
}

void *maintain_database()
{
    void *db = NULL;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    while(true == ready_to_work){
        zlog_info(category_debug, 
                  "SQL_retain_data with database_keep_days=[%d]", 
                  config.database_keep_days); 
        SQL_retain_data(db, config.database_keep_days * HOURS_EACH_DAY);

        zlog_info(category_debug, "SQL_vacuum_database");
        SQL_vacuum_database(db);

        //Sleep one day before next check
        sleep_t(86400 * 1000);
    }

    SQL_close_database_connection(db);

    return (void *)NULL;
}

void *Server_summarize_location_information(){
    void *db = NULL;
    int uptime = 0;
    int last_sync_location_timestamp = 0;
    int last_sync_movement_timestamp = 0;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    uptime = get_clock_time();

    while(true == ready_to_work){
    
        uptime = get_clock_time();
        
        if(uptime - last_sync_location_timestamp >=
            config.period_between_check_object_location){
           
            last_sync_location_timestamp = uptime;

            /* Compute each object's location within time interval:
               1. Compute each object's lbeacon_uuid that has strongest rssi 
                  of this object
               2. Compute the stay of length time of this object under this 
                  lbeacon_uuid
            */
            SQL_summarize_object_location(db,
                                          config.location_time_interval_in_sec);

            if(config.is_enabled_panic_button_monitor){
                // Check each object's panic_button status within time interval
                SQL_identify_panic(db, 
                                   config.panic_time_interval_in_sec);
            }

            if(config.is_enabled_geofence_monitor){
                // Check each object's geo-fence status within time interval
                SQL_identify_geo_fence(db, 
                                       config.geofence_monitor_config.
                                       geo_fence_time_interval_in_sec);
            }
        }

        if(config.is_enabled_movement_monitor){
            if(uptime - last_sync_movement_timestamp >= 
                config.period_between_check_object_activity){
    
                last_sync_movement_timestamp = uptime;

                SQL_identify_last_movement_status(
                    db, 
                    config.movement_monitor_config.time_interval_in_min, 
                    config.movement_monitor_config.each_time_slot_in_min,
                    config.movement_monitor_config.rssi_delta);    
            }
        }

        sleep_t(NORMAL_WAITING_TIME_IN_MS);
    }

    SQL_close_database_connection(db);

    return (void *)NULL;
}


void *Server_collect_violation_event(){
    void *db = NULL;
    int uptime = 0;
    int last_collect_violation_events_timestamp = 0;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    uptime = get_clock_time();

    while(true == ready_to_work){
    
        uptime = get_clock_time();
        
        if(config.is_enabled_collect_violation_event){
            if(uptime - last_collect_violation_events_timestamp >=
                config.collect_violation_event_time_interval_in_sec){
           
                last_collect_violation_events_timestamp = uptime;

                if(config.is_enabled_geofence_monitor){
                    SQL_collect_violation_events(
                        db,
                        MONITOR_GEO_FENCE,
                        config.geofence_monitor_config.
                        geo_fence_time_interval_in_sec,
                        config.granularity_for_continuous_violations_in_sec);
                }
                if(config.is_enabled_panic_button_monitor){
                    SQL_collect_violation_events(
                        db,
                        MONITOR_PANIC,
                        config.panic_time_interval_in_sec,
                        config.granularity_for_continuous_violations_in_sec);
                }
                if(config.is_enabled_movement_monitor){
                }
            }
        }

        sleep_t(BUSY_WAITING_TIME_IN_MS);
    }

    SQL_close_database_connection(db);

    return (void *)NULL;
}

void *Server_NSI_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    char gateway_record[WIFI_MESSAGE_LENGTH];

    void *db = NULL;

    JoinStatus join_status = JOIN_UNKNOWN;

    zlog_info(category_debug, "Start join...(%s)", 
              current_node -> net_address);

    memset(gateway_record, 0, sizeof(gateway_record));

    sprintf(gateway_record, "1;%s;%d;", current_node -> net_address,
            S_NORMAL_STATUS);
    
    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;

    }

    SQL_update_gateway_registration_status(db, gateway_record,
                                           strlen(gateway_record));

    SQL_update_lbeacon_registration_status(db,
                                           current_node->content,
                                           strlen(current_node->content),
                                           current_node -> net_address);

    SQL_close_database_connection(db);


     /* Put the address into Gateway_address_map */
    if (true == Gateway_join_request(&Gateway_address_map, 
                                     current_node -> net_address) ){
        join_status = JOIN_ACK;
    }    
    else{
        join_status = JOIN_DENY;
    }

    current_node -> pkt_direction = from_server;
    current_node -> pkt_type = join_response;

    sprintf(current_node->content, "%d;%d;%s;%d;", current_node->pkt_direction, 
                                                   current_node->pkt_type,
                                                   BOT_SERVER_API_VERSION_LATEST,
                                                   join_status);

    current_node->content_size = strlen(current_node->content);

    pthread_mutex_lock(&NSI_send_buffer_list_head.list_lock);

    insert_list_tail( &current_node -> buffer_entry,
                      &NSI_send_buffer_list_head.list_head);

    pthread_mutex_unlock( &NSI_send_buffer_list_head.list_lock);

    zlog_info(category_debug, "%s join success", current_node -> net_address);
    
    return (void *)NULL;
}


void *Server_BHM_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
    
    void *db = NULL;
    
    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    if(current_node->pkt_direction == from_gateway){

        if(current_node->pkt_type == gateway_health_report){
          
            SQL_update_gateway_health_status(db,
                                             current_node -> content,
                                             current_node -> content_size,
                                             current_node -> net_address);
        }
        else if(current_node->pkt_type == beacon_health_report){

            SQL_update_lbeacon_health_status(db,
                                             current_node -> content,
                                             current_node -> content_size,
                                             current_node -> net_address);
        }
    }
    SQL_close_database_connection(db);
    

    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}

void *Server_LBeacon_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
    
    void *db = NULL;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    if(current_node -> pkt_type == tracked_object_data)
    {
        // Server should support backward compatibility.
        if(atof(BOT_SERVER_API_VERSION_20) == current_node -> API_version){
            SQL_update_object_tracking_data(db,
                                            current_node -> content,
                                            strlen(current_node -> content));
        }else{
            SQL_update_object_tracking_data_with_battery_voltage(
                db,
                current_node -> content,
                strlen(current_node -> content));
        }

    }

    SQL_close_database_connection(db);

    mp_free( &node_mempool, current_node);

    return (void* )NULL;
}


void *process_tracked_data_from_geofence_gateway(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
   
    void *db = NULL;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;

    }

    if(current_node -> pkt_type == time_critical_tracked_object_data){
        
        check_geo_fence_violations(current_node);

        // Server should support backward compatibility.
        if(atof(BOT_SERVER_API_VERSION_20) == current_node -> API_version){
            SQL_update_object_tracking_data(db,
                                            current_node -> content,
                                            strlen(current_node -> content));
        }else{
            SQL_update_object_tracking_data_with_battery_voltage(
                db,
                current_node -> content,
                strlen(current_node -> content));
        }
        
    }

    SQL_close_database_connection(db);

    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}


void *process_GeoFence_alert_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
  
    void *db = NULL;

    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    SQL_insert_geo_fence_alert(db, current_node -> content, 
                               current_node -> content_size);

    SQL_close_database_connection(db);
    
    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}



bool Gateway_join_request(AddressMapArray *address_map, char *address)
{
    int not_in_use = -1;
    int n;
    int answer = -1;

    zlog_info(category_debug, 
              "Enter Gateway_join_request address [%s]", address);

    pthread_mutex_lock( &address_map -> list_lock);
    /* Copy all the necessary information received from the LBeacon to the
       address map. */

    /* Find the first unused address map location and use the location to store
       the new joined LBeacon. */

    zlog_info(category_debug, "Check whether joined");

    answer = is_in_Address_Map(address_map, address, 0);
    if(answer >=0)
    {
        memset(address_map ->address_map_list[answer].net_address, 0, 
               NETWORK_ADDR_LENGTH);
        strncpy(address_map -> address_map_list[answer].net_address,
                address, strlen(address));

        address_map -> address_map_list[answer].last_request_time =
            get_system_time();

        pthread_mutex_unlock( &address_map -> list_lock);
        zlog_info(category_debug, "Exist and Return");

        return true;
    }

    for(n = 0 ; n < MAX_NUMBER_NODES ; n ++){
        if(address_map -> in_use[n] == false && not_in_use == -1)
        {
            not_in_use = n;
            break;
        }
    }

    zlog_info(category_debug, "Start join...(%s)", address);

    /* If still has space for the LBeacon to register */
    if (not_in_use != -1)
    {
        address_map -> in_use[not_in_use] = true;

        memset(address_map->address_map_list[not_in_use].net_address, 0, 
               NETWORK_ADDR_LENGTH);
        strncpy(address_map->address_map_list[not_in_use].net_address, 
                address, strlen(address));

        pthread_mutex_unlock( &address_map -> list_lock);

        zlog_info(category_debug, "Join Success");

        return true;
    }
    else
    {
        pthread_mutex_unlock( &address_map -> list_lock);

        zlog_info(category_debug, "Join maximum");
        return false;
    }
}


void Broadcast_to_gateway(AddressMapArray *address_map, char *msg, int size)
{
    /* The counter for for-loop*/
    int current_index;

    pthread_mutex_lock( &address_map -> list_lock);

    if (size <= WIFI_MESSAGE_LENGTH)
    {
        for(current_index = 0;
            current_index < MAX_NUMBER_NODES;
            current_index ++)
        {
            if (address_map -> in_use[current_index] == true)
            {
                /* Add the content of the buffer node to the UDP to be sent to
                   the server */
                udp_addpkt( &udp_config,
                            address_map ->
                            address_map_list[current_index].net_address,
                            config.send_port,
                            msg,
                            size);
            }
        }
    }

    pthread_mutex_unlock( &address_map -> list_lock);
}


void *Server_process_wifi_send(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
    char content[WIFI_MESSAGE_LENGTH];

#ifdef debugging
    zlog_info(category_debug, 
              "Start Send pkt\naddress [%s]\nport [%d]\nmsg [%s]\nsize [%d]",
              current_node->net_address,
              config.send_port,
              current_node->content,
              current_node->content_size);
#endif

    memset(content, 0, WIFI_MESSAGE_LENGTH);

    sprintf(content, "%d;%d;%s;%s", current_node->pkt_direction, 
                                    current_node->pkt_type,
                                    BOT_SERVER_API_VERSION_LATEST,
                                    current_node->content);

    strcpy(current_node->content, content);
    current_node -> content_size =  strlen(current_node->content);
  
    /* Add the content of the buffer node to the UDP to be sent to the 
       destination */
    udp_addpkt(&udp_config, 
               current_node -> net_address, 
               current_node -> port,
               current_node -> content,
               current_node -> content_size);

    mp_free( &node_mempool, current_node);

    zlog_info(category_debug, "Send Success");

    return (void *)NULL;
}


void *Server_process_wifi_receive()
{
    BufferNode *new_node;

    sPkt temppkt;

    char buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char *from_direction = NULL;
    char *request_type = NULL;
    char *API_version = NULL;
    char *remain_string = NULL;

    while (ready_to_work == true)
    {
        temppkt = udp_getrecv( &udp_config);

        /* If there is no pkt received */
        if(temppkt.is_null == true)
        {
            sleep_t(BUSY_WAITING_TIME_IN_MS);
            continue;
        }

        /* Allocate memory from node_mempool a buffer node for received data
           and copy the data from Wi-Fi receive queue to the node. */
        new_node = NULL;

        new_node = mp_alloc( &node_mempool);
        
        if(NULL == new_node){
             zlog_info(category_debug, 
                       "Server_process_wifi_receive (new_node) mp_alloc " \
                       "failed, abort this data");
             continue;
        }

        memset(new_node, 0, sizeof(BufferNode));

        /* Initialize the entry of the buffer node */
        init_entry( &new_node -> buffer_entry);

        memset(buf, 0, sizeof(buf));
        strcpy(buf, temppkt.content);

        remain_string = buf;

        from_direction = strtok_save(buf, DELIMITER_SEMICOLON, &saveptr);
        if(from_direction == NULL)
        {
             mp_free( &node_mempool, new_node);
             continue;
        }
        remain_string = remain_string + strlen(from_direction) + 
                        strlen(DELIMITER_SEMICOLON);         
        sscanf(from_direction, "%d", &new_node -> pkt_direction);
     
        request_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        if(request_type == NULL)
        {
             mp_free( &node_mempool, new_node);
             continue;
        }
        remain_string = remain_string + strlen(request_type) + 
                        strlen(DELIMITER_SEMICOLON);
        sscanf(request_type, "%d", &new_node -> pkt_type);

        API_version = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        if(API_version == NULL)
        {
             mp_free( &node_mempool, new_node);
             continue;
        }
        remain_string = remain_string + strlen(API_version) + 
                        strlen(DELIMITER_SEMICOLON);
        sscanf(API_version, "%f", &new_node -> API_version);
       
        /* Copy the content to the buffer_node */
        strcpy(new_node->content, remain_string);
        zlog_debug(category_debug, "pkt_direction=[%d], pkt_type=[%d], " \
                   "API_version=[%f], content=[%s]", new_node->pkt_direction, 
                   new_node->pkt_type, new_node->API_version, 
                   new_node->content);

        new_node -> content_size = strlen(new_node->content);

        new_node -> port = temppkt.port;

        memcpy(new_node -> net_address, temppkt.address,    
               NETWORK_ADDR_LENGTH);

        /* Insert the node to the specified buffer, and release
           list_lock. */

        if (from_gateway == new_node -> pkt_direction) 
        {
            switch (new_node -> pkt_type) 
            {
                case request_to_join:
#ifdef debugging
                    display_time();
#endif
                    zlog_info(category_debug, "Get Join request from "
                              "Gateway");

                    pthread_mutex_lock(&NSI_receive_buffer_list_head
                                       .list_lock);
                    insert_list_tail(&new_node -> buffer_entry,
                                     &NSI_receive_buffer_list_head
                                     .list_head);
                    pthread_mutex_unlock(&NSI_receive_buffer_list_head
                                         .list_lock);
                    break;

                case time_critical_tracked_object_data:
#ifdef debugging
                    display_time();
#endif
                    zlog_info(category_debug, "Get tracked object data from "
                              "geofence Gateway");
                    zlog_info(category_debug, "new_node->content=[%s]", 
                              new_node->content);

                    pthread_mutex_lock(&Geo_fence_receive_buffer_list_head
                                       .list_lock);
                    insert_list_tail(&new_node -> buffer_entry,
                                     &Geo_fence_receive_buffer_list_head
                                     .list_head);
                    pthread_mutex_unlock(&Geo_fence_receive_buffer_list_head
                                         .list_lock);

                    break;

                case tracked_object_data:
#ifdef debugging
                    display_time();
#endif
                    zlog_info(category_debug, "Get Tracked Object Data from "
                              "normal Gateway");

                    pthread_mutex_lock(&LBeacon_receive_buffer_list_head
                                       .list_lock);
                    insert_list_tail(&new_node -> buffer_entry, 
                                     &LBeacon_receive_buffer_list_head
                                     .list_head);
                    pthread_mutex_unlock(&LBeacon_receive_buffer_list_head
                                         .list_lock);
                        
                    break;

                case gateway_health_report:
                case beacon_health_report:
#ifdef debugging
                    display_time();
#endif
                    zlog_info(category_debug, "Get Health Report from " \
                                              "Gateway");

                    pthread_mutex_lock(&BHM_receive_buffer_list_head
                                       .list_lock);
                    insert_list_tail(&new_node -> buffer_entry,
                                     &BHM_receive_buffer_list_head.list_head);
                    pthread_mutex_unlock(&BHM_receive_buffer_list_head
                                         .list_lock);
                    break;
                default:
                    mp_free( &node_mempool, new_node);
                    break;
            }
        }
        else
        {
            mp_free( &node_mempool, new_node);
        }
    }
    return (void *)NULL;
}


ErrorCode add_geo_fence_settings(struct List_Entry *geo_fence_list_head, 
                                char *buf)
{
    char *current_ptr = NULL;
    char *save_ptr = NULL;

    char *name = NULL;
    char *unique_key = NULL;
    char *hours_duration = NULL;
    char *perimeters = NULL;
    char *fences = NULL;
    char *monitor_types = NULL;
    char *hour_start = NULL;
    char *hour_end = NULL;
    void *db = NULL;
    char perimeters_config[CONFIG_BUFFER_SIZE];
    char fences_config[CONFIG_BUFFER_SIZE];

    GeoFenceListNode *new_node = NULL;

    int i = 0;
    char *temp_value = NULL;

    memset(perimeters_config, 0, sizeof(perimeters_config));
    memset(fences_config, 0, sizeof(fences_config));

    zlog_info(category_debug, ">> add_geo_fence_settings");
    zlog_info(category_debug, "GeoFence data=[%s]", buf);

    name = strtok_save(buf, DELIMITER_SEMICOLON, &save_ptr);

    unique_key = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    hours_duration = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    perimeters = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
    strcpy(perimeters_config, perimeters);
    
    fences = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
    strcpy(fences_config, fences);

    monitor_types = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    zlog_info(category_debug, 
              "name=[%s], perimeters=[%s], fences=[%s], monitor_types=[%s]", 
              name, perimeters, fences, monitor_types);

    new_node = mp_alloc( &geofence_mempool);
           
    if(NULL == new_node){
        zlog_error(category_health_report,
                   "[add_geo_fence_settings] mp_alloc failed, abort this data");

        return E_MALLOC;
    }

    memset(new_node, 0, sizeof(GeoFenceListNode));

    init_entry(&new_node -> geo_fence_list_entry);
                    
    memcpy(new_node->name, name, strlen(name));

    memcpy(new_node->unique_key, unique_key, strlen(unique_key));

    // parse hours duration
    hour_start = strtok_save(hours_duration, DELIMITER_COMMA, &save_ptr);
    hour_end = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);

    // parse perimeters settings
    current_ptr = strtok_save(perimeters, DELIMITER_COMMA, &save_ptr);
    new_node->number_perimeters = atoi (current_ptr);
    if(new_node->number_perimeters > 
       MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER)
    {
        zlog_error(category_debug,
                   "number_perimeters[%d] exceeds our maximum support number[%d]",
                   new_node->number_perimeters,
                   MAXIMUM_LBEACONS_IN_GEO_FENCE_PERIMETER);
    }
    else
    {
        if(new_node->number_perimeters > 0){
            for(i = 0 ; i < new_node->number_perimeters ; i++){
                temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
                strcpy(new_node->perimeters[i], temp_value);
            }
            current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
            new_node->rssi_of_perimeters = atoi(current_ptr);
        }
    }

    // parse fences settings
    current_ptr = strtok_save(fences, DELIMITER_COMMA, &save_ptr);
    new_node->number_fences = atoi (current_ptr);
    if(new_node->number_fences > 
       MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE)
    {
        zlog_error(category_debug,
                   "number_fences[%d] exceeds our maximum support number[%d]",
                   new_node->number_fences,
                   MAXIMUM_LBEACONS_IN_GEO_FENCE_FENCE);
    }
    else
    {
        if(new_node->number_fences > 0){
            for(i = 0 ; i < new_node->number_fences ; i++){
                temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
                strcpy(new_node->fences[i], temp_value);
            }
            current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
            new_node->rssi_of_fences = atoi(current_ptr);
        }
    }
    
    insert_list_tail( &new_node -> geo_fence_list_entry,
                      geo_fence_list_head);


    // update geo-fence configuration to database
    if(WORK_SUCCESSFULLY != 
       SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                   "cannot open database"); 
        return E_SQL_OPEN_DATABASE;
    }

    zlog_debug(category_debug, 
               "perimeter = [%s], fences = [%s]", 
               perimeters_config, fences_config);

    SQL_update_geo_fence_config(db, unique_key, name, 
                                perimeters_config, fences_config, 
                                hour_start, hour_end);
    
    SQL_close_database_connection(db);

 
    zlog_info(category_debug, "<<add_geo_fence_settings");

    return WORK_SUCCESSFULLY;
}


ErrorCode check_geo_fence_violations(BufferNode *buffer_node)
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
    char *lbeacon_datetime = NULL;
    char *lbeacon_ip = NULL;
    char *object_type = NULL;
    char *string_number_objects = NULL;
    char *mac_address = NULL;
    char *initial_timestamp = NULL;
    char *final_timestamp = NULL;
    char *rssi = NULL;
    char *panic_button = NULL;
    char *battery_voltage = NULL;

    int i = 0;
    int number_types = 0;
    int number_objects = 0;
    int detected_rssi = 0;

    bool is_perimeter_lbeacon = false;
    bool is_fence_lbeacon = false;

    void *db = NULL;
    ObjectMonitorType object_monitor_type = MONITOR_NORMAL;

    int is_rule_enabled = 0;
    int rule_hour_start = 0;
    int rule_hour_end = 0;
    int current_hour = 0;

    time_t current_time = get_system_time();
    struct tm ts;
    char string_hour[80];

    ts = *localtime(&current_time);
    strftime(string_hour, sizeof(string_hour), "%H", &ts);
    current_hour = atoi(string_hour);


    zlog_info(category_debug, ">>check_geo_fence_violations");

    
    // get current hour
    ts = *localtime(&current_time);
    strftime(string_hour, sizeof(string_hour), "%H", &ts);
    current_hour = atoi(string_hour);

 
    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    lbeacon_uuid = strtok_save(content_temp, DELIMITER_SEMICOLON, &save_ptr);
    

    list_for_each(current_list_entry, &(config.geo_fence_list_head)){

        current_list_ptr = ListEntry(current_list_entry,
                                     GeoFenceListNode,
                                     geo_fence_list_entry);

        if(WORK_SUCCESSFULLY != 
           SQL_open_database_connection(database_argument, &db)){

            zlog_error(category_debug, 
                       "cannot open database"); 
            continue;
        }

        SQL_get_geo_fence_config(db, 
                                 current_list_ptr->unique_key, 
                                 &is_rule_enabled,
                                 &rule_hour_start,
                                 &rule_hour_end);

        SQL_close_database_connection(db);

        if(is_rule_enabled == 0){
            zlog_debug(category_debug, 
                       "Skip geo-fence rule=[%s], enable=[%d]", 
                       current_list_ptr->unique_key, is_rule_enabled);
            continue;
        }

        if(rule_hour_start < rule_hour_end){
            if(current_hour < rule_hour_start || 
                rule_hour_end < current_hour){

                zlog_debug(category_debug, 
                           "Skip geo-fence rule=[%s], start=[%d], end=[%d], " \
                           "current_hour=[%d]", 
                           current_list_ptr->unique_key, rule_hour_start,
                           rule_hour_end, current_hour);
                continue;
            }
        }else{
            if(rule_hour_end < current_hour && 
                current_hour < rule_hour_start){
                zlog_debug(category_debug, 
                           "Skip geo-fence rule=[%s], start=[%d], end=[%d], " \
                           "current_hour=[%d]", 
                           current_list_ptr->unique_key, rule_hour_start,
                           rule_hour_end, current_hour);
                continue;
            }
        }

        // Check if lbeacon_uuid is listed as perimeter LBeacon or 
        // fence LBeacon of this geo-fence setting
        is_perimeter_lbeacon = false;
        is_fence_lbeacon = false;

        for(i = 0 ; i < current_list_ptr->number_perimeters ; i++){
            if(0 == strncmp_caseinsensitive(lbeacon_uuid, 
                                            current_list_ptr->perimeters[i], 
                                            strlen(lbeacon_uuid))){
                is_perimeter_lbeacon = true;
                break;
            }
        }

        for(i = 0 ; i < current_list_ptr->number_fences ; i++){
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
                     "lbeacon_uuid=[%s] is not in geo-fence setting name=[%s]", 
                     lbeacon_uuid, current_list_ptr->name);

            continue;
        }
        
        memset(content_temp, 0, WIFI_MESSAGE_LENGTH);
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

        // We set number_types as 2, because we need to parse BLE and 
        // BR_EDR types
        number_types = 2;
        while(number_types --){
            object_type = strtok_save(NULL, 
                                      DELIMITER_SEMICOLON, 
                                      &save_ptr);
        
            string_number_objects = strtok_save(NULL, 
                                            DELIMITER_SEMICOLON, 
                                            &save_ptr);
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

                panic_button = strtok_save(NULL, 
                                           DELIMITER_SEMICOLON, 
                                           &save_ptr);

                if(atof(BOT_SERVER_API_VERSION_20) == buffer_node->API_version){
                    /* We do not have additional fields in 
                       BOT_SERVER_API_VERSION_20, the all fields are already 
                       listed above */
                       
                }else{
                    battery_voltage = strtok_save(NULL, 
                                                  DELIMITER_SEMICOLON, 
                                                  &save_ptr);
                }
       
                detected_rssi = atoi(rssi);
                
                if(WORK_SUCCESSFULLY != 
                    SQL_open_database_connection(database_argument, &db)){

                    zlog_error(category_debug, 
                               "cannot open database"); 
                    continue;
                }

                SQL_get_object_monitor_type(db, mac_address, &object_monitor_type);

                SQL_close_database_connection(db);
                
                if(MONITOR_GEO_FENCE != 
                   (MONITOR_GEO_FENCE & 
                    (ObjectMonitorType)object_monitor_type)){
                    continue;
                }

                if(is_fence_lbeacon && 
                   detected_rssi > 
                   current_list_ptr->rssi_of_fences){

                    zlog_info(category_debug, 
                              "[GeoFence-Fence]: LBeacon UUID=[%s] "\
                              "mac_address=[%s]", 
                              lbeacon_uuid, mac_address);


                    insert_into_geo_fence_alert_list(
                        mac_address,
                        current_list_ptr->name, 
                        GEO_FENCE_ALERT_TYPE_FENCE,
                        lbeacon_uuid,
                        final_timestamp,
                        rssi);

                }else if(is_perimeter_lbeacon && 
                         detected_rssi > 
                         current_list_ptr->rssi_of_perimeters){

                    zlog_info(category_debug, 
                              "[GeoFence-Perimeter]: LBeacon UUID=[%s]" \
                              "mac_address=[%s]", 
                              lbeacon_uuid, mac_address);


                    insert_into_geo_fence_alert_list(
                        mac_address,
                        current_list_ptr->name, 
                        GEO_FENCE_ALERT_TYPE_PERIMETER,
                        lbeacon_uuid,
                        final_timestamp,
                        rssi);

                }
            }
        }
    }


    zlog_info(category_debug, "<<check_geo_fence_violations");


    return WORK_SUCCESSFULLY;
}

ErrorCode insert_into_geo_fence_alert_list(char *mac_address, 
                                           char *fence_name,
                                           char *fence_type, 
                                           char *lbeacon_uuid, 
                                           char *final_timestamp, 
                                           char *rssi){
        BufferNode *new_node = NULL;


        zlog_info(category_debug, ">> insert_into_geo_fence_alert_list");


        new_node = mp_alloc( &node_mempool);
        if(NULL == new_node){
            zlog_info(category_debug,
                      "[insert_into_geo_fence_alert_list] mp_alloc failed, abort "\
                      "this data");
            return E_MALLOC;
        }

        memset(new_node, 0, sizeof(BufferNode));

        init_entry( &new_node -> buffer_entry);

        sprintf(new_node -> content, "%d;%s;%s;%s;%s;%s;%s;", 1, 
                                                           mac_address, 
                                                           fence_name,
                                                           fence_type, 
                                                           lbeacon_uuid, 
                                                           final_timestamp, 
                                                           rssi);

        new_node -> content_size = strlen(new_node -> content);
        
        pthread_mutex_lock(&Geo_fence_alert_buffer_list_head.list_lock);

        insert_list_tail( &new_node -> buffer_entry,
                          &Geo_fence_alert_buffer_list_head.list_head);

        pthread_mutex_unlock(&Geo_fence_alert_buffer_list_head.list_lock); 


        zlog_info(category_debug, "<< insert_into_geo_fence_alert_list");


        return WORK_SUCCESSFULLY;
 }


