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
     gateways through Wi-Fi network, and programs executed by network setup and
     initialization, beacon health monitor and comminication unit.

  Version:

     1.0, 20191002

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

    /* The command message to be sent */
    char command_msg[WIFI_MESSAGE_LENGTH];

    int uptime;

    /* The main thread of the communication Unit */
    pthread_t CommUnit_thread;

    /* The thread of database maintenance */
    pthread_t database_maintenance_thread;

    /* The thread for summarizing location information */
    pthread_t location_information_thread;

    /* The thread for monitoring object violations */
    pthread_t monitor_object_violation_thread;

    /* The thread for reloading monitoring configuration */
    pthread_t reload_monitor_config_thread;

    /* The thread for collecting violation events */
    pthread_t collect_violation_thread;

    /* The thread for sending notification */
    pthread_t send_notification_thread;

    /* The thread to listen for messages from Wi-Fi interface */
    pthread_t wifi_listener_thread;

    /* Initialize flags */
    NSI_initialization_complete      = false;
    CommUnit_initialization_complete = false;
    initialization_failed            = false;
    ready_to_work                    = true;


    /* Initialize zlog */
    if(zlog_init(ZLOG_CONFIG_FILE_NAME) == 0)
    {
        category_health_report = 
            zlog_get_category(LOG_CATEGORY_HEALTH_REPORT);

        if (!category_health_report)
            zlog_fini();

        category_debug = zlog_get_category(LOG_CATEGORY_DEBUG);
        if (!category_debug)
            zlog_fini();
    }

    zlog_info(category_debug,"Start Server");

    zlog_info(category_debug,"Mempool Initializing");

    /* Initialize the memory pool for buffer nodes */
    if(MEMORY_POOL_SUCCESS != mp_init( &node_mempool, 
                                       sizeof(BufferNode), 
                                       SLOTS_IN_MEM_POOL_BUFFER_NODE))
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool for geo-fence area node structs */
    if(MEMORY_POOL_SUCCESS != mp_init( &geofence_area_mempool, 
                                       sizeof(GeoFenceAreaNode), 
                                       SLOTS_IN_MEM_POOL_GEO_FENCE_AREA))
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool for geo-fence setting node structs */
    if(MEMORY_POOL_SUCCESS != mp_init( &geofence_setting_mempool, 
                                       sizeof(GeoFenceSettingNode), 
                                       SLOTS_IN_MEM_POOL_GEO_FENCE_SETTING))
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool for mac_address of objects under geo-fence area settings */
    if(MEMORY_POOL_SUCCESS != mp_init( &geofence_objects_area_mempool, 
                                       sizeof(ObjectWithGeoFenceAreaNode), 
                                       SLOTS_IN_MEM_POOL_GEO_FENCE_OBJECTS_SETTING))
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool for mac_address of objects under geo-fence area settings */
    if(MEMORY_POOL_SUCCESS != mp_init( &geofence_violation_mempool, 
                                       sizeof(GeoFenceViolationNode), 
                                       SLOTS_IN_MEM_POOL_GEO_FENCE_VIOLATIONS))
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool for notification structs */
    if(MEMORY_POOL_SUCCESS != mp_init( &notification_mempool, 
                                       sizeof(NotificationListNode), 
                                       SLOTS_IN_MEM_POOL_NOTIFICATION))
    {
        return E_MALLOC;
    }

    zlog_info(category_debug,"Mempool Initialized");

    /* Create the config from input serverconfig file */

    if(WORK_SUCCESSFULLY != get_server_config( &config, 
                                               &common_config, 
                                               CONFIG_FILE_NAME))
    {
        zlog_error(category_health_report, "Opening config file Fail");
        zlog_error(category_debug, "Opening config file Fail");
        return E_OPEN_FILE;
    }

    zlog_info(category_debug,"Initialize buffer lists");

    /* Initialize the address map*/
    init_Address_Map( &Gateway_address_map);

    /* Initialize buffer_list_heads and add to the head in to the priority 
       list.
     */
    init_buffer( &priority_list_head, (void *) sort_priority_list,
                common_config.high_priority);

    init_buffer( &command_buffer_list_head,
                (void *) process_commands,
                common_config.normal_priority);
    insert_list_tail( &command_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &Geo_fence_receive_buffer_list_head,
                (void *) process_tracked_data_from_geofence_gateway, 
                common_config.time_critical_priority);
    insert_list_tail( &Geo_fence_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &data_receive_buffer_list_head,
                (void *) Server_LBeacon_routine, 
                common_config.normal_priority);
    insert_list_tail( &data_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_send_buffer_list_head,
                (void *) Server_process_wifi_send, 
                common_config.high_priority);
    insert_list_tail( &NSI_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_receive_buffer_list_head,
                (void *) Server_NSI_routine, 
                common_config.high_priority);
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

    /* Initialize the list of database connection */
    init_entry( &(config.db_connection_list_head.list_head));

    pthread_mutex_init( &config.db_connection_list_head.list_lock, 0);

    /* Initialize the list of geo-fences */
    init_entry( &(config.geo_fence_list_head.list_head));

    pthread_mutex_init( &config.geo_fence_list_head.list_lock, 0);

    /* Initialize the list of objects under geo fence monitoring */
    init_entry( &(config.objects_under_geo_fence_list_head.list_head));

    pthread_mutex_init( &config.objects_under_geo_fence_list_head.list_lock, 0);

    /* Initialize the list of geo fence violation records */
    init_entry( &(config.geo_fence_violation_list_head.list_head));

    pthread_mutex_init( &config.geo_fence_violation_list_head.list_lock, 0);


    /* Create database connection pool */
    zlog_info(category_debug,"Initialize database connection pool");

    if(WORK_SUCCESSFULLY != 
       SQL_create_database_connection_pool(database_argument, 
                                           &config.db_connection_list_head, 
                                           config.number_of_database_connection)){ 
       
            SQL_destroy_database_connection_pool(
                &config.db_connection_list_head);

            zlog_error(category_debug, 
                       "Failed to initialize database connection pool");
            return E_SQL_OPEN_DATABASE;
    }

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

    if(config.is_enabled_geofence_monitor){
        construct_geo_fence_list(&config.db_connection_list_head, 
                                 &config.geo_fence_list_head,
                                 true,
                                 0);
    
        construct_objects_list_under_geo_fence_monitoring(
            &config.db_connection_list_head, 
            &config.objects_under_geo_fence_list_head,
            true,
            0);
    }
    zlog_info(category_debug,"Initiaize geo-fence list and objects");

    zlog_info(category_debug,"Initialize Communication Unit");

    /* Create the main thread of Communication Unit  */
    return_value = startThread( &CommUnit_thread, CommUnit_routine, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "CommUnit_thread Create Fail");
        zlog_error(category_debug, "CommUnit_thread Create Fail");
        return return_value;
    }

    /* Create thread to maintain database */
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

    /* Create thread to monitor object violations */
    return_value = startThread( &monitor_object_violation_thread, 
                                Server_monitor_object_violations, 
                                NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, 
                   "Server_monitor_object_violations fail");
        zlog_error(category_debug, 
                   "Server_monitor_object_violations fail");
        return return_value;
    }

    /* Create thread to reload monitoring configuration */
    return_value = startThread( &reload_monitor_config_thread, 
                                Server_reload_monitor_config, 
                                NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, 
                   "Server_reload_monitor_config fail");
        zlog_error(category_debug, 
                   "Server_reload_monitor_config fail");
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

    /* Create thread to collect notification events */
    return_value = startThread( &send_notification_thread, 
                                Server_send_notification, 
                                NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, 
                   "Server_send_notification fail");
        zlog_error(category_debug, 
                   "Server_send_notification fail");
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


        /* If it is the time to poll track object data from LBeacons, do it */
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
            broadcast_to_gateway(&Gateway_address_map, command_msg,
                                 strlen(command_msg));

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
            broadcast_to_gateway(&Gateway_address_map, command_msg,
                                 strlen(command_msg));

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

    SQL_destroy_database_connection_pool(&config.db_connection_list_head);

    if(config.is_enabled_geofence_monitor){
        destroy_geo_fence_list(&config.geo_fence_list_head,
                               true,
                               0);

        destroy_objects_list_under_geo_fence_monitoring(
            &config.objects_under_geo_fence_list_head,
            true,
            0);
    }
    
    mp_destroy(&geofence_area_mempool);

    mp_destroy(&geofence_setting_mempool);

    mp_destroy(&geofence_objects_area_mempool);

    mp_destroy(&geofence_violation_mempool);

    mp_destroy(&notification_mempool);

    return WORK_SUCCESSFULLY;
}


ErrorCode get_server_config(ServerConfig *config, 
                            CommonConfig *common_config, 
                            char *file_name) 
{
    FILE *file = fopen(file_name, "r");
    char config_message[CONFIG_BUFFER_SIZE];
    int number_geofence_settings = 0;
    int number_location_not_stay_room_settings = 0;
    int number_location_long_stay_settings = 0;
    int number_movement_settings = 0;
    int number_notification_settings = 0;
    int i = 0;
    char *save_ptr = NULL;

    List_Entry *current_list_entry = NULL;
    GeoFenceSettingNode *current_list_ptr = NULL;

    if (file == NULL) 
    {
        zlog_error(category_health_report, "Load serverconfig fail");
        zlog_info(category_debug, "Load serverconfig fail");
        return E_OPEN_FILE;
    }

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->server_installation_path, config_message,
           sizeof(config->server_installation_path));
    zlog_info(category_debug,"Server Installation Path [%s]", 
              config->server_installation_path);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->server_ip, config_message, sizeof(config->server_ip));
    zlog_info(category_debug,"Server IP [%s]", config->server_ip);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    memcpy(config->db_ip, config_message, sizeof(config->db_ip));
    zlog_info(category_debug,"Database IP [%s]", config->db_ip);

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
    config->period_between_check_object_movement_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "period_between_check_object_movement_in_sec [%d]",
              config->period_between_check_object_movement_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->server_localtime_against_UTC_in_hour = atoi(config_message);
    zlog_info(category_debug,
              "server_localtime_against_UTC_in_hour [%d]",
              config->server_localtime_against_UTC_in_hour);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->number_worker_threads = atoi(config_message);
    zlog_info(category_debug,
              "Number of worker threads [%d]",
              common_config->number_worker_threads);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    common_config->min_age_out_of_date_packet_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "min_age_out_of_date_packet_in_sec in seconds [%d]",
              common_config->min_age_out_of_date_packet_in_sec);

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
    config->database_keep_hours = atoi(config_message);
    zlog_info(category_debug,
              "Database database_keep_hours [%d]", 
              config->database_keep_hours);

    fetch_next_string(file, config_message, sizeof(config_message));
    config->number_of_database_connection = atoi(config_message);
    zlog_info(category_debug,
              "The number_of_database_connection is [%d]",
              config->number_of_database_connection);
        
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
    config->database_pre_filter_time_window_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "The database_pre_filter_time_window_in_sec is [%d]", 
              config->database_pre_filter_time_window_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->location_time_interval_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "The location_time_interval_in_sec is [%d]", 
              config->location_time_interval_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message));
    config->rssi_difference_of_stable_tag = atoi(config_message);
    zlog_info(category_debug,
              "The rssi_difference_of_stable_tag is [%d]",
              config->rssi_difference_of_stable_tag);

    fetch_next_string(file, config_message, sizeof(config_message));
    config->rssi_difference_of_location_accuracy_tolerance = 
        atoi(config_message);
    zlog_info(category_debug,
              "The rssi_difference_of_location_accuracy_tolerance is [%d]",
              config->rssi_difference_of_location_accuracy_tolerance);

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
    config->perimeter_valid_duration_in_sec = atoi(config_message);
    zlog_info(category_debug,
              "The perimeter_valid_duration_in_sec is [%d]", 
              config->perimeter_valid_duration_in_sec);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_location_monitor = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_location_monitor is [%d]", 
              config->is_enabled_location_monitor);
    
    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_movement_monitor = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_movement_monitor is [%d]", 
              config->is_enabled_movement_monitor);

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->movement_monitor_config.monitor_interval_in_min = atoi(config_message);
    zlog_info(category_debug,
              "The time_interval_in_min is [%d]", 
              config->movement_monitor_config.
              monitor_interval_in_min);

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
    config->collect_violation_event_time_interval_in_sec = atoi(config_message);
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

    fetch_next_string(file, config_message, sizeof(config_message)); 
    config->is_enabled_send_notification_alarm = atoi(config_message);
    zlog_info(category_debug,
              "The is_enabled_send_notification_alarm is [%d]", 
              config->is_enabled_send_notification_alarm);

    zlog_info(category_debug, "Initialize notification list");

    /* Initialize notification list head to store all the notification 
       settings */
    init_entry( &(config->notification_list_head));

    fetch_next_string(file, config_message, sizeof(config_message)); 
    number_notification_settings = atoi(config_message);

    for(i = 0; i < number_notification_settings ; i++){
          
        fetch_next_string(file, config_message, sizeof(config_message)); 
        add_notification_to_the_notification_list(
            &(config->notification_list_head), 
            config_message);
    }
       
    zlog_info(category_debug, "notification list initialized");


    fclose(file);

    return WORK_SUCCESSFULLY;
}

void *maintain_database()
{
    ErrorCode ret = WORK_SUCCESSFULLY;

    while(true == ready_to_work){
        zlog_info(category_debug, 
                  "SQL_delete_old_data with database_keep_hours=[%d]", 
                  config.database_keep_hours); 

        ret = SQL_delete_old_data(&config.db_connection_list_head, 
                                  config.database_keep_hours);

        if(WORK_SUCCESSFULLY != ret){
            zlog_error(category_debug, 
                       "SQL_delete_old_data failed ret=[%d]", 
                       ret); 

        }

        zlog_info(category_debug, "SQL_vacuum_database");

        ret = SQL_vacuum_database(&config.db_connection_list_head);

        if(WORK_SUCCESSFULLY != ret){
            zlog_error(category_debug, 
                       "SQL_vacuum_database failed ret=[%d]", 
                       ret); 
        }

        //Sleep one hour before next check
        sleep_t(MS_EACH_HOUR);
    }

    return (void *)NULL;
}

void *Server_summarize_location_information(){
    
    while(true == ready_to_work){
    
        SQL_summarize_object_location(&config.db_connection_list_head,
                                      config.database_pre_filter_time_window_in_sec,
                                      config.location_time_interval_in_sec,
                                      config.rssi_difference_of_stable_tag,
                                      config.rssi_difference_of_location_accuracy_tolerance);

        sleep_t(BUSY_WAITING_TIME_IN_MS);
    }

    return (void *)NULL;
}

void *Server_monitor_object_violations(){
    int uptime = 0;
    int last_monitor_movement_timestamp = 0;
   

    uptime = get_clock_time();
    
    while(true == ready_to_work){
    
        uptime = get_clock_time();

        if(config.is_enabled_location_monitor){
                
            SQL_identify_location_not_stay_room(
                &config.db_connection_list_head);
 
            SQL_identify_location_long_stay_in_danger(
                &config.db_connection_list_head);
        }

        if(config.is_enabled_movement_monitor &&
           (uptime - last_monitor_movement_timestamp >= 
            config.period_between_check_object_movement_in_sec)){
    
            last_monitor_movement_timestamp = uptime;

            SQL_identify_last_movement_status(
                &config.db_connection_list_head, 
                config.movement_monitor_config.monitor_interval_in_min, 
                config.movement_monitor_config.each_time_slot_in_min,
                config.movement_monitor_config.rssi_delta);    
        }
       
        sleep_t(BUSY_WAITING_TIME_IN_MS);
    }

    return (void *)NULL;
}


void *Server_reload_monitor_config(){
    
    while(true == ready_to_work){
       
        SQL_reload_monitor_config(&config.db_connection_list_head, 
                                  config.server_localtime_against_UTC_in_hour);

        sleep_t(NORMAL_WAITING_TIME_IN_MS);

    }

    return (void*) NULL;
    
}

void *Server_collect_violation_event(){

    while(true == ready_to_work){

        if(config.is_enabled_collect_violation_event){
          
            if(config.is_enabled_geofence_monitor){
                SQL_collect_violation_events(
                    &config.db_connection_list_head,
                    MONITOR_GEO_FENCE,
                    config.collect_violation_event_time_interval_in_sec,
                    config.granularity_for_continuous_violations_in_sec);
            }
            if(config.is_enabled_panic_button_monitor){
                SQL_collect_violation_events(
                    &config.db_connection_list_head,
                    MONITOR_PANIC,
                    config.collect_violation_event_time_interval_in_sec,
                    config.granularity_for_continuous_violations_in_sec);
            }
            if(config.is_enabled_movement_monitor){
                SQL_collect_violation_events(
                    &config.db_connection_list_head,
                    MONITOR_MOVEMENT,
                    config.collect_violation_event_time_interval_in_sec,
                    config.granularity_for_continuous_violations_in_sec);
            }
            if(config.is_enabled_location_monitor){
                SQL_collect_violation_events(
                    &config.db_connection_list_head,
                    MONITOR_LOCATION,
                    config.collect_violation_event_time_interval_in_sec,
                    config.granularity_for_continuous_violations_in_sec);
            }
        }
      
        sleep_t(BUSY_WAITING_TIME_IN_MS);
    }

    return (void *)NULL;
}


void *Server_send_notification(){
    char violation_info[WIFI_MESSAGE_LENGTH];


    while(true == ready_to_work){

        if(config.is_enabled_send_notification_alarm){

            memset(violation_info, 0, sizeof(violation_info));

            SQL_get_and_update_violation_events(
                &config.db_connection_list_head, 
                violation_info, 
                sizeof(violation_info));

            /* The notification alarm is sent out to all BOT agents currently.
               If needed, we can extend notification feature to support 
               granularity. */
            if(strlen(violation_info) > 0){
                zlog_debug(category_debug, "send notification for [%s]", 
                           violation_info);
                send_notification_alarm_to_gateway();            
            }
        }

        sleep_t(BUSY_WAITING_TIME_IN_MS);
    }

    return (void *)NULL;
}

void send_notification_alarm_to_gateway(){

    List_Entry * current_list_entry = NULL;
    NotificationListNode *current_list_ptr = NULL;
    char command_msg[WIFI_MESSAGE_LENGTH];

    list_for_each(current_list_entry, &(config.notification_list_head)){

        current_list_ptr = ListEntry(current_list_entry,
                                     NotificationListNode,
                                     notification_list_entry);

        memset(command_msg, 0, sizeof(command_msg));

        sprintf(command_msg, "%d;%d;%s;%d;%d;%s;", 
                from_server, 
                notification_alarm, 
                BOT_SERVER_API_VERSION_LATEST,
                current_list_ptr->alarm_type,
                current_list_ptr->alarm_duration_in_sec,
                current_list_ptr->agents_list);

        udp_addpkt(&udp_config,
                   current_list_ptr->gateway_ip,
                   config.send_port,
                   command_msg,
                   strlen(command_msg));
    }
}

void *Server_NSI_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    char gateway_record[WIFI_MESSAGE_LENGTH];

    JoinStatus join_status = JOIN_UNKNOWN;

    zlog_info(category_debug, "Start join...(%s)", 
              current_node -> net_address);

    memset(gateway_record, 0, sizeof(gateway_record));

    sprintf(gateway_record, "1;%s;%d;", current_node -> net_address,
            S_NORMAL_STATUS);
   
    SQL_update_gateway_registration_status(
        &config.db_connection_list_head, 
        gateway_record,
        strlen(gateway_record));

    SQL_update_lbeacon_registration_status(
        &config.db_connection_list_head,
        current_node->content,
        strlen(current_node->content),
        current_node -> net_address);

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

    sprintf(current_node->content, "%d;", join_status);

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
    
   
    if(current_node->pkt_direction == from_gateway){

        if(current_node->pkt_type == gateway_health_report){
          
            SQL_update_gateway_health_status(&config.db_connection_list_head,
                                             current_node -> content,
                                             current_node -> content_size,
                                             current_node -> net_address);
        }
        else if(current_node->pkt_type == beacon_health_report){

            SQL_update_lbeacon_health_status(&config.db_connection_list_head,
                                             current_node -> content,
                                             current_node -> content_size,
                                             current_node -> net_address);
        }
    }

    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}

void *Server_LBeacon_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
    

    if(current_node -> pkt_type == tracked_object_data)
    {
        // Server should support backward compatibility.
        if(atof(BOT_SERVER_API_VERSION_20) == current_node -> API_version){
            /*[obsoleted-20200109]
            SQL_update_object_tracking_data(db,
                                            current_node -> content,
                                            strlen(current_node -> content));
                                            */
        }else{
            SQL_update_object_tracking_data_with_battery_voltage(
                &config.db_connection_list_head,
                current_node -> content,
                strlen(current_node -> content),
                config.server_installation_path,
                config.is_enabled_panic_button_monitor);
        }

    }

    mp_free( &node_mempool, current_node);

    return (void* )NULL;
}



void *process_commands(void *_buffer_node){
    BufferNode *current_node = (BufferNode *)_buffer_node;
    char buf[WIFI_MESSAGE_LENGTH];
    char *save_ptr = NULL;
    char *ipc_command = NULL;
    IPCCommand command = CMD_NONE;

    zlog_debug(category_debug, ">>process_commands [%s]", 
               current_node->content);
   
    memset(buf, 0, sizeof(buf));
    strcpy(buf, current_node->content);
    ipc_command = strtok_save(buf, DELIMITER_SEMICOLON, &save_ptr);
    if(ipc_command != NULL){
        command = (IPCCommand) atoi(ipc_command);
        switch (command){
            case CMD_RELOAD_GEO_FENCE_SETTING:
                reload_geo_fence_settings(current_node->content, 
                                          &config.db_connection_list_head,
                                          &config.geo_fence_list_head, 
                                          &config.objects_under_geo_fence_list_head);
                break;
            default:
                break;
        }
    }

    mp_free( &node_mempool, current_node);

    zlog_debug(category_debug, "<<process_commands");

    return (void* )NULL;
}

void *process_tracked_data_from_geofence_gateway(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    
    if(current_node -> pkt_type == time_critical_tracked_object_data){
       
        if(config.is_enabled_geofence_monitor){

            check_geo_fence_violations(current_node, 
                                       &config.db_connection_list_head,
                                       &config.geo_fence_list_head, 
                                       &config.objects_under_geo_fence_list_head,
                                       &config.geo_fence_violation_list_head,
                                       config.perimeter_valid_duration_in_sec,
                                       config.granularity_for_continuous_violations_in_sec);
        }

        // Server should support backward compatibility.
        if(atof(BOT_SERVER_API_VERSION_20) == current_node -> API_version){
            /*[obsoleted-20200109]
            SQL_update_object_tracking_data(db,
                                            current_node -> content,
                                            strlen(current_node -> content));
                                            */
        }else{
            SQL_update_object_tracking_data_with_battery_voltage(
                &config.db_connection_list_head,
                current_node -> content,
                strlen(current_node -> content),
                config.server_installation_path,
                config.is_enabled_panic_button_monitor);
        }
        
    }

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
        /* Need to update last request time for each gateway */
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


void broadcast_to_gateway(AddressMapArray *address_map, char *msg, int size)
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

    int retry_times = 0;
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
            sleep_t(BUSY_WAITING_TIME_IN_WIFI_REXEIVE_PACKET_IN_MS);
            continue;
        }

        /* Allocate memory from node_mempool a buffer node for received data
           and copy the data from Wi-Fi receive queue to the node. */
        new_node = NULL;

        retry_times = MEMORY_ALLOCATE_RETRIES;
        while(retry_times --){
            new_node = mp_alloc( &node_mempool);

            if(NULL != new_node)
                break;
        }
        if(NULL == new_node){
             zlog_info(category_debug, 
                       "Server_process_wifi_receive (new_node) mp_alloc " \
                       "failed, abort this data");
             continue;
        }

        memset(new_node, 0, sizeof(BufferNode));

        /* Initialize the entry of the buffer node */
        init_entry( &new_node -> buffer_entry);

        new_node -> uptime_at_receive = get_clock_time();

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
                   "API_version=[%f]", new_node->pkt_direction, 
                   new_node->pkt_type, new_node->API_version);

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

                    pthread_mutex_lock(&data_receive_buffer_list_head
                                       .list_lock);
                    insert_list_tail(&new_node -> buffer_entry, 
                                     &data_receive_buffer_list_head
                                     .list_head);
                    pthread_mutex_unlock(&data_receive_buffer_list_head
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
        else if(from_gui == new_node -> pkt_direction){
            switch(new_node -> pkt_type){
                case ipc_command:
#ifdef debugging
                    display_time();
#endif
                    zlog_info(category_debug, "Get IPC command from " \
                                              "GUI");
                    pthread_mutex_lock(&command_buffer_list_head.list_lock);
                    insert_list_tail(&new_node -> buffer_entry,
                                     &command_buffer_list_head.list_head);
                    pthread_mutex_unlock(&command_buffer_list_head.list_lock);
                    break;
                default:
                    mp_free( &node_mempool, new_node);
                    break;
            }
        }else{
            mp_free( &node_mempool, new_node);
        }
    }
    return (void *)NULL;
}



ErrorCode add_notification_to_the_notification_list(
    struct List_Entry * notification_list_head,
    char *buf){

    char *current_ptr = NULL;
    char *save_ptr = NULL;

    char *alarm_type = NULL;
    char *alarm_duration = NULL;
    char *gateway_ip = NULL;
    char *agents_list = NULL;

    NotificationListNode *new_node = NULL;

    int i;
    char *temp_value = NULL;
    int retry_times;


    zlog_info(category_debug, ">> add_notification_to_the_notification_list");
    zlog_info(category_debug, "Notification data=[%s]", buf);

    alarm_type = strtok_save(buf, DELIMITER_SEMICOLON, &save_ptr);

    alarm_duration = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    gateway_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    agents_list = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    zlog_info(category_debug, 
              "alarm_type=[%s], gateway_ip=[%s], agents_list=[%s]", 
              alarm_type, gateway_ip, agents_list);

    retry_times = MEMORY_ALLOCATE_RETRIES;
    while(retry_times --){
        new_node = mp_alloc( &notification_mempool);
    
        if(NULL != new_node)
            break;
    }
           
    if(NULL == new_node){
        zlog_error(category_health_report,
                   "[add_notification_to_the_notification_list] " \
                   "mp_alloc failed, abort this data");

        return E_MALLOC;
    }

    memset(new_node, 0, sizeof(NotificationListNode));

    init_entry(&new_node -> notification_list_entry);
                    
    new_node->alarm_type = (AlarmType)atoi(alarm_type);

    new_node->alarm_duration_in_sec = atoi(alarm_duration);
    
    strcpy(new_node->gateway_ip, gateway_ip);

    strcpy(new_node->agents_list, agents_list);
    
    insert_list_tail( &new_node -> notification_list_entry,
                      notification_list_head);
 
    zlog_info(category_debug, "<<add_notification_to_the_notification_list");

    return WORK_SUCCESSFULLY;
}


