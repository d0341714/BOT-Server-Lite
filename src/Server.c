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

#ifdef debugging
    zlog_info(category_debug,"Start Server");
#endif

#ifdef debugging
    zlog_info(category_debug,"Mempool Initializing");
#endif

    /* Initialize the memory pool */
    if(mp_init( &node_mempool, sizeof(BufferNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS)
    {
        return E_MALLOC;
    }

    /* Initialize the memory pool */
    if(mp_init( &geofence_mempool, sizeof(GeoFenceListNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS)
    {
        return E_MALLOC;
    }

#ifdef debugging
    zlog_info(category_debug,"Mempool Initialized");
#endif

    /* Create the config from input serverconfig file */

    if(get_server_config( &config, CONFIG_FILE_NAME) != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "Opening config file Fail");
#ifdef debugging
        zlog_error(category_debug, "Opening config file Fail");
#endif
        return E_OPEN_FILE;
    }

#ifdef debugging
    zlog_info(category_debug,"Start connect to Database");
#endif

    /* Open DB */

    memset(database_argument, 0, SQL_TEMP_BUFFER_LENGTH);

    sprintf(database_argument, "dbname=%s user=%s password=%s host=%s port=%d",
                               config.database_name, 
                               config.database_account,
                               config.database_password, 
                               config.db_ip,
                               config.database_port );

#ifdef debugging
    zlog_info(category_debug,"Initialize buffer lists");
#endif

    /* Initialize the address map*/
    init_Address_Map( &Gateway_address_map);

    /* Initialize buffer_list_heads and add to the head in to the priority list.
     */
    init_buffer( &priority_list_head, (void *) sort_priority_list,
                config.high_priority);

    init_buffer( &Geo_fence_alert_buffer_list_head,
                (void *) process_GeoFence_alert_routine, 
                config.time_critical_priority);
    insert_list_tail( &Geo_fence_alert_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &Geo_fence_receive_buffer_list_head,
                (void *) process_tracked_data_from_geofence_gateway, 
                config.time_critical_priority);
    insert_list_tail( &Geo_fence_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &LBeacon_receive_buffer_list_head,
                (void *) Server_LBeacon_routine, config.normal_priority);
    insert_list_tail( &LBeacon_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_send_buffer_list_head,
                (void *) Server_process_wifi_send, config.normal_priority);
    insert_list_tail( &NSI_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_receive_buffer_list_head,
                (void *) Server_NSI_routine, config.normal_priority);
    insert_list_tail( &NSI_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_receive_buffer_list_head,
                (void *) Server_BHM_routine, config.low_priority);
    insert_list_tail( &BHM_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_send_buffer_list_head,
                (void *) Server_process_wifi_send, config.low_priority);
    insert_list_tail( &BHM_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    sort_priority_list(&config, &priority_list_head);

#ifdef debugging
    zlog_info(category_debug,"Buffer lists initialize");
#endif

#ifdef debugging
    zlog_info(category_debug,"Initialize sockets");
#endif

    /* Initialize the Wifi connection */
    if(udp_initial( &udp_config, config.recv_port) != WORK_SUCCESSFULLY){

        /* Error handling and return */
        initialization_failed = true;
        zlog_error(category_health_report, "Fail to initialize sockets");
        
#ifdef debugging
        zlog_error(category_debug, "Fail to initialize sockets");
#endif

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

#ifdef debugging
    zlog_info(category_debug,"Sockets initialized");
#endif

    NSI_initialization_complete = true;

#ifdef debugging
    zlog_info(category_debug,"Network Setup and Initialize success");
#endif

#ifdef debugging
    zlog_info(category_debug,"Initialize Communication Unit");
#endif

    /* Create the main thread of Communication Unit  */
    return_value = startThread( &CommUnit_thread, CommUnit_routine, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "CommUnit_thread Create Fail");
#ifdef debugging
        zlog_error(category_debug, "CommUnit_thread Create Fail");
#endif
        return return_value;
    }

    /* Create thread to main database */
    return_value = startThread( &database_maintenance_thread, maintain_database, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "Maintain database fail");
#ifdef debugging
        zlog_error(category_debug, "Maintain database fail");
#endif
        return return_value;
    }

    /* Create thread to summarize location informtion */
    return_value = startThread( &location_information_thread, summarize_location_information, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        zlog_error(category_health_report, "summarize_location_information fail");
#ifdef debugging
        zlog_error(category_debug, "summarize_location_information fail");
#endif
        return return_value;
    }

#ifdef debugging
    zlog_info(category_debug,"Start Communication");
#endif

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

    last_update_geo_fence = 0;

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
            /* Pull object tracking object data */
            /* set the pkt type */

            memset(command_msg, 0, WIFI_MESSAGE_LENGTH);
            sprintf(command_msg, "%d;%d;%s;", from_server, 
                                              tracked_object_data, 
                                              BOT_SERVER_API_VERSION);

#ifdef debugging
            display_time();
            zlog_info(category_debug,"Send Request for Tracked Object Data");
#endif

            /* Broadcast poll messenge to gateways */
            Broadcast_to_gateway(&Gateway_address_map, command_msg,
                                 WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_object_tracking_time */
            last_polling_object_tracking_time = uptime;
        }

        /* Since period_between_RFTOD is short, we only allow one type 
           of data to be sent at the same time except for tracked object data. 
         */
        if(uptime - last_polling_LBeacon_for_HR_time >=
                config.period_between_RFHR)
        {
            /* Polling for health reports. */
            /* set the pkt type */

            memset(command_msg, 0, WIFI_MESSAGE_LENGTH);
            sprintf(command_msg, "%d;%d;%s;", from_server, 
                                              gateway_health_report, 
                                              BOT_SERVER_API_VERSION);

#ifdef debugging
            display_time();
            zlog_info(category_debug,"Send Request for Health Report");
#endif

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


ErrorCode get_server_config(ServerConfig *config, char *file_name) 
{
    FILE *file = fopen(file_name, "r");
    char config_setting[CONFIG_BUFFER_SIZE];
    char *config_message = NULL;
    int config_message_size = 0;
    int number_geofence_settings = 0;
    int i = 0;

    List_Entry *current_list_entry = NULL;
    GeoFenceListNode *current_list_ptr = NULL;

    if (file == NULL) 
    {
        zlog_error(category_health_report, "Load serverconfig fail");
#ifdef debugging
        zlog_info(category_debug, "Load serverconfig fail");
#endif
        return E_OPEN_FILE;
    }
    else 
    {
        /* Keep reading each line and store into the serverconfig struct */
        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->server_ip, config_message, config_message_size);

#ifdef debugging
        zlog_info(category_debug,"Server IP [%s]", config->server_ip);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->db_ip, config_message, config_message_size);

#ifdef debugging
        zlog_info(category_debug,"Database IP [%s]", config->db_ip);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->allowed_number_nodes = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,"Allow Number of Nodes [%d]", 
               config->allowed_number_nodes);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_RFHR = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "Periods between request for health report " \
                  "period_between_RFHR [%d]",
                  config->period_between_RFHR);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_RFTOD = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "Periods between request for tracked object data " \
                  "period_between_RFTOD [%d]",
                  config->period_between_RFTOD);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_check_object_location = 
            atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "period_between_check_object_location [%d]",
                  config->period_between_check_object_location);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_check_object_activity = 
            atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "period_between_check_object_activity [%d]",
                  config->period_between_check_object_activity);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->number_worker_threads = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "Number of worker threads [%d]",
                  config->number_worker_threads);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->send_port = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The destination port when sending [%d]", 
                  config->send_port);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->recv_port = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The received port [%d]", config->recv_port);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->database_port = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug, 
                  "The database port [%d]", config->database_port);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->database_name, config_message, config_message_size)
        ;

#ifdef debugging
        zlog_info(category_debug,
                  "Database Name [%s]", config->database_name);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->database_account, config_message, 
               config_message_size);

#ifdef debugging
        zlog_info(category_debug,
                  "Database Account [%s]", config->database_account);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->database_password, config_message, 
               config_message_size);


        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->database_keep_days = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "Database database_keep_days [%d]", 
                  config->database_keep_days);
#endif
        
        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->time_critical_priority = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The nice of time critical priority is [%d]",
                  config->time_critical_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->high_priority = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The nice of high priority is [%d]", 
                  config->high_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->normal_priority = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The nice of normal priority is [%d]",
                  config->normal_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->low_priority = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The nice of low priority is [%d]", 
                  config->low_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->location_time_interval_in_sec = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The location_time_interval_in_sec is [%d]", 
                  config->location_time_interval_in_sec);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->panic_time_interval_in_sec = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The panic_time_interval_in_sec is [%d]", 
                  config->panic_time_interval_in_sec);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->geo_fence_time_interval_in_sec = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The geo_fence_time_interval_in_sec is [%d]", 
                  config->geo_fence_time_interval_in_sec);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->inactive_time_interval_in_min = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The inactive_time_interval_in_min is [%d]", 
                  config->inactive_time_interval_in_min);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->inactive_each_time_slot_in_min = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The inactive_each_time_slot_in_min is [%d]", 
                  config->inactive_each_time_slot_in_min);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->inactive_rssi_delta = atoi(config_message);

#ifdef debugging
        zlog_info(category_debug,
                  "The inactive_rssi_delta is [%d]", 
                  config->inactive_rssi_delta);
#endif

#ifdef debugging
        zlog_info(category_debug, "Initialize geo-fence list");
#endif

       /* Initialize geo-fence list head to store all the geo-fence settings */
       init_entry( &(config->geo_fence_list_head));

       fgets(config_setting, sizeof(config_setting), file);
       config_message = strstr((char *)config_setting, DELIMITER);
       config_message = config_message + strlen(DELIMITER);
       trim_string_tail(config_message);
       number_geofence_settings = atoi(config_message);

       for(i = 0; i < number_geofence_settings ; i++){
          
           fgets(config_setting, sizeof(config_setting), file);
           config_message = strstr((char *)config_setting, DELIMITER);
           config_message = config_message + strlen(DELIMITER);
           trim_string_tail(config_message);
        
           add_geo_fence_setting(&(config->geo_fence_list_head), config_message);
       }

#ifdef debugging
       list_for_each(current_list_entry, &(config->geo_fence_list_head)){
            current_list_ptr = ListEntry(current_list_entry,
                                         GeoFenceListNode,
                                         geo_fence_list_entry);
            
            zlog_info(category_debug, "name=[%s]", current_list_ptr->name);

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
       
#ifdef debugging
        zlog_info(category_debug, "geo-fence list initialized");
#endif

        fclose(file);

    }
    return WORK_SUCCESSFULLY;
}


void *sort_priority_list(ServerConfig *config, BufferListHead *list_head)
{
    List_Entry *list_pointer,
               *next_list_pointer;

    List_Entry critical_priority_head, high_priority_head,
               normal_priority_head, low_priority_head;

    BufferListHead *current_head, *next_head;

    init_entry( &critical_priority_head);
    init_entry( &high_priority_head);
    init_entry( &normal_priority_head);
    init_entry( &low_priority_head);

    pthread_mutex_lock( &list_head -> list_lock);

    list_for_each_safe(list_pointer, next_list_pointer,
                       &list_head -> priority_list_entry)
    {

        remove_list_node(list_pointer);

        current_head = ListEntry(list_pointer, BufferListHead,
                                 priority_list_entry);

        if(current_head -> priority_nice == config -> time_critical_priority)

            insert_list_tail( list_pointer, &critical_priority_head);

        else if(current_head -> priority_nice == config -> high_priority)

            insert_list_tail( list_pointer, &high_priority_head);

        else if(current_head -> priority_nice == config -> 
                normal_priority)

            insert_list_tail( list_pointer, &normal_priority_head);

        else if(current_head -> priority_nice == config -> low_priority)

            insert_list_tail( list_pointer, &low_priority_head);

    }

    if(is_entry_list_empty(&critical_priority_head) == false)
    {
        list_pointer = critical_priority_head.next;
        remove_list_node(list_pointer -> prev);
        concat_list( &list_head -> priority_list_entry, list_pointer);
    }

    if(is_entry_list_empty(&high_priority_head) == false)
    {
        list_pointer = high_priority_head.next;
        remove_list_node(list_pointer -> prev);
        concat_list( &list_head -> priority_list_entry, list_pointer);
    }

    if(is_entry_list_empty(&normal_priority_head) == false)
    {
        list_pointer = normal_priority_head.next;
        remove_list_node(list_pointer -> prev);
        concat_list( &list_head -> priority_list_entry, list_pointer);
    }

    if(is_entry_list_empty(&low_priority_head) == false)
    {
        list_pointer = low_priority_head.next;
        remove_list_node(list_pointer -> prev);
        concat_list( &list_head -> priority_list_entry, list_pointer);
    }

    pthread_mutex_unlock( &list_head -> list_lock);


    return (void *)NULL;
}


int udp_sendpkt(pudp_config udp_config, BufferNode *buffer_node)
{
    char content[WIFI_MESSAGE_LENGTH];

    memset(content, 0, WIFI_MESSAGE_LENGTH);

    sprintf(content, "%d;%d;%s;%s", buffer_node->pkt_direction, 
                                    buffer_node->pkt_type,
                                    BOT_SERVER_API_VERSION,
                                    buffer_node->content);

    strcpy(buffer_node->content, content);
    buffer_node -> content_size =  strlen(buffer_node->content);
  
    /* Add the content of the buffer node to the UDP to be sent to the 
       destination */
    udp_addpkt(udp_config, 
               buffer_node -> net_address, 
               buffer_node -> port,
               content,
               buffer_node -> content_size);

    return WORK_SUCCESSFULLY;
}


void *maintain_database()
{
    void *db = NULL;

    if(WORK_SUCCESSFULLY != SQL_open_database_connection(database_argument, &db)){
        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    while(true == ready_to_work){
        zlog_info(category_debug, 
                  "SQL_retain_data with database_keep_days=[%d]", 
                  config.database_keep_days); 
        SQL_retain_data(db, config.database_keep_days * 24);

        zlog_info(category_debug, "SQL_vacuum_database");
        SQL_vacuum_database(db);

        //Sleep one day before next check
        sleep_t(86400 * 1000);
    }

    SQL_close_database_connection(db);

    return (void *)NULL;
}

void *summarize_location_information(){
    void *db = NULL;
    int uptime = 0;
    int last_sync_location = 0;
    int last_sync_activity = 0;

    if(WORK_SUCCESSFULLY != SQL_open_database_connection(database_argument, &db)){
        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    uptime = get_clock_time();

    while(true == ready_to_work){
    
        uptime = get_clock_time();
        
        if(uptime - last_sync_location >=
            config.period_between_check_object_location){
            
            SQL_summarize_object_inforamtion(
                db, 
                config.location_time_interval_in_sec,
                config.panic_time_interval_in_sec,
                config.geo_fence_time_interval_in_sec);
        }

        if(uptime - last_sync_activity >= 
            config.period_between_check_object_activity){
        
            SQL_identify_last_activity_status(
                db, 
                config.inactive_time_interval_in_min, 
                config.inactive_each_time_slot_in_min,
                config.inactive_rssi_delta);
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

#ifdef debugging
    zlog_info(category_debug, "Start join...(%s)", 
              current_node -> net_address);
#endif

    memset(gateway_record, 0, sizeof(gateway_record));

    sprintf(gateway_record, "1;%s;%d;", current_node -> net_address,
            S_NORMAL_STATUS);
    
    if(WORK_SUCCESSFULLY != SQL_open_database_connection(database_argument, &db)){

        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;

    }

    SQL_update_gateway_registration_status(db, gateway_record,
                                           strlen(gateway_record));

    SQL_update_lbeacon_registration_status(db,
                                           current_node->content,
                                           strlen(current_node->content));

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
                                                  BOT_SERVER_API_VERSION,
                                                  join_status);

    current_node->content_size = strlen(current_node->content);

    pthread_mutex_lock(&NSI_send_buffer_list_head.list_lock);

    insert_list_tail( &current_node -> buffer_entry,
                      &NSI_send_buffer_list_head.list_head);

    pthread_mutex_unlock( &NSI_send_buffer_list_head.list_lock);

#ifdef debugging
    zlog_info(category_debug, "%s join success", current_node -> net_address);
#endif
    
    return (void *)NULL;
}


void *Server_BHM_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
    
    void *db = NULL;
    
    if(WORK_SUCCESSFULLY != SQL_open_database_connection(database_argument, &db)){
        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    if(current_node->pkt_direction == from_gateway){

        if(current_node->pkt_type == gateway_health_report){
          
            SQL_update_gateway_health_status(db,
                                             current_node -> content,
                                             current_node -> content_size);
        }
        else if(current_node->pkt_type == beacon_health_report){

            SQL_update_lbeacon_health_status(db,
                                             current_node -> content,
                                             current_node -> content_size);
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

    if(WORK_SUCCESSFULLY != SQL_open_database_connection(database_argument, &db)){
        zlog_error(category_debug, 
                  "cannot open database"); 
        return (void *)NULL;
    }

    if(current_node -> pkt_type == tracked_object_data)
    {

        SQL_update_object_tracking_data(db,
                                        current_node -> content,
                                        strlen(current_node -> content));

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

        SQL_update_object_tracking_data(db,
                                        current_node -> content,
                                        strlen(current_node -> content));
        
    }

    SQL_close_database_connection(db);

    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}


void *process_GeoFence_alert_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;
  
    void *db = NULL;

    SQL_open_database_connection(database_argument, &db);

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
    int answer;

#ifdef debugging
    zlog_info(category_debug, 
              "Enter Gateway_join_request address [%s]", address);
#endif

    pthread_mutex_lock( &address_map -> list_lock);
    /* Copy all the necessary information received from the LBeacon to the
       address map. */

    /* Find the first unused address map location and use the location to store
       the new joined LBeacon. */

#ifdef debugging
    zlog_info(category_debug, "Check whether joined");
#endif

    if(answer = is_in_Address_Map(address_map, address, 0) >=0)
    {
        strncpy(address_map -> address_map_list[answer].net_address,
                address, NETWORK_ADDR_LENGTH);

        address_map -> address_map_list[answer].last_request_time =
            get_system_time();

        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        zlog_info(category_debug, "Exist and Return");
#endif

        return true;
    }

    for(n = 0 ; n < MAX_NUMBER_NODES ; n ++){
        if(address_map -> in_use[n] == false && not_in_use == -1)
        {
            not_in_use = n;
            break;
        }
    }

#ifdef debugging
    zlog_info(category_debug, "Start join...(%s)", address);
#endif

    /* If still has space for the LBeacon to register */
    if (not_in_use != -1)
    {
        AddressMap *tmp =  &address_map -> address_map_list[not_in_use];

        address_map -> in_use[not_in_use] = true;

        strncpy(tmp -> net_address, address, NETWORK_ADDR_LENGTH);

        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        zlog_info(category_debug, "Join Success");
#endif

        return true;
    }
    else
    {
        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        zlog_info(category_debug, "Join maximum");
#endif
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

#ifdef debugging
    zlog_info(category_debug, 
              "Start Send pkt\naddress [%s]\nport [%d]\nmsg [%s]\nsize [%d]",
              current_node->net_address,
              config.send_port,
              current_node->content,
              current_node->content_size);
#endif

    /* Add the content of the buffer node to the UDP to be sent to the
       server */
    //udp_sendpkt(&udp_config, 
    //            current_node -> net_address,config.send_port, 
    //            current_node -> content, current_node -> content_size);
    udp_sendpkt(&udp_config, current_node);

    mp_free( &node_mempool, current_node);

#ifdef debugging
    zlog_info(category_debug, "Send Success");
#endif

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
                       "Server_process_wifi_receive (new_node) mp_alloc failed," \
                       " abort this data");
             continue;
        }

        memset(new_node, 0, sizeof(BufferNode));

        /* Initialize the entry of the buffer node */
        init_entry( &new_node -> buffer_entry);

        memset(buf, 0, sizeof(buf));
        strcpy(buf, temppkt.content);

        remain_string = buf;

        from_direction = strtok_save(buf, DELIMITER_SEMICOLON, &saveptr);
        remain_string = remain_string + strlen(from_direction) + strlen(DELIMITER_SEMICOLON);         

        new_node -> pkt_direction = atoi(from_direction);
     
        request_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        remain_string = remain_string + strlen(request_type) + strlen(DELIMITER_SEMICOLON);

        new_node -> pkt_type = atoi(request_type);

        API_version = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        remain_string = remain_string + strlen(API_version) + strlen(DELIMITER_SEMICOLON);

        /* Copy the content to the buffer_node */
        strcpy(new_node->content, remain_string);
        zlog_debug(category_debug, "pkt_direction=[%d], pkt_type=[%d], content=[%s]", 
                   new_node->pkt_direction, new_node->pkt_type, new_node->content);

        new_node -> content_size = strlen(new_node->content);

        new_node -> port = temppkt.port;

        memcpy(new_node -> net_address, temppkt.address,    
               NETWORK_ADDR_LENGTH);

        /* Insert the node to the specified buffer, and release
           list_lock. */

        switch (new_node -> pkt_direction) 
        {
            case from_gateway:

                switch (new_node -> pkt_type) 
                {
                    case request_to_join:
#ifdef debugging
                        display_time();
                        zlog_info(category_debug, "Get Join request from Gateway");
#endif
                        pthread_mutex_lock( 
                                       &NSI_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                       &NSI_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                       &NSI_receive_buffer_list_head.list_lock);
                        break;

                    case time_critical_tracked_object_data:
#ifdef debugging
                        display_time();
                        zlog_info(category_debug, "Get tracked object data from geofence Gateway");
                        zlog_info(category_debug, "new_node->content=[%s]", new_node->content);
#endif
                        pthread_mutex_lock(
                                  &Geo_fence_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                  &Geo_fence_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                  &Geo_fence_receive_buffer_list_head.list_lock);

                        break;

                    case tracked_object_data:
#ifdef debugging
                        display_time();
                        zlog_info(category_debug, "Get Tracked Object Data from normal Gateway");
#endif            
                        pthread_mutex_lock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                   &LBeacon_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                        
                        break;

                    case gateway_health_report:
                    case beacon_health_report:
#ifdef debugging
                        display_time();
                        zlog_info(category_debug, "Get Health Report from Gateway");
#endif
                        pthread_mutex_lock( 
                                       &BHM_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                       &BHM_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                       &BHM_receive_buffer_list_head.list_lock);
                        break;
                    default:
                        mp_free( &node_mempool, new_node);
                        break;
                }
                    
                break;

            default:
                mp_free( &node_mempool, new_node);
                break;
        }
    }
    return (void *)NULL;
}


ErrorCode add_geo_fence_setting(struct List_Entry *geo_fence_list_head, 
                                char *buf)
{
    char *current_ptr = NULL;
    char *save_ptr = NULL;

    char *name = NULL;
    char *perimeters = NULL;
    char *fences = NULL;
    char *mac_prefixes = NULL;

    GeoFenceListNode *new_node = NULL;

    int i = 0;
    char *temp_value = NULL;

#ifdef debugging
    zlog_info(category_debug, ">> add_geo_fence_setting");
    zlog_info(category_debug, "GeoFence data=[%s]", buf);
#endif

    name = strtok_save(buf, DELIMITER_SEMICOLON, &save_ptr);

    perimeters = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
    
    fences = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    mac_prefixes = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

#ifdef debugging
    zlog_info(category_debug, 
              "name=[%s], perimeters=[%s], fences=[%s], mac_prefixes=[%s]", 
              name, perimeters, fences, mac_prefixes);
#endif

    new_node = mp_alloc( &geofence_mempool);
           
    if(NULL == new_node){
        zlog_error(category_health_report,
                   "[add_geo_fence_setting] mp_alloc failed, abort this data");

        return E_MALLOC;
    }

    memset(new_node, 0, sizeof(GeoFenceListNode));

    init_entry(&new_node -> geo_fence_list_entry);
                    
    memcpy(new_node->name, name, strlen(name));
  
    // parse perimeters settings
    current_ptr = strtok_save(perimeters, DELIMITER_COMMA, &save_ptr);
    new_node->number_perimeters = atoi (current_ptr);
    if(new_node->number_perimeters > 0){
        for(i = 0 ; i < new_node->number_perimeters ; i++){
            temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
            strcpy(new_node->perimeters[i], temp_value);
        }
        current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
        new_node->rssi_of_perimeters = atoi(current_ptr);
    }

    // parse fences settings
    current_ptr = strtok_save(fences, DELIMITER_COMMA, &save_ptr);
    new_node->number_fences = atoi (current_ptr);
    if(new_node->number_fences > 0){
        for(i = 0 ; i < new_node->number_fences ; i++){
            temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
            strcpy(new_node->fences[i], temp_value);
        }
        current_ptr = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
        new_node->rssi_of_fences = atoi(current_ptr);
    }

    // parse mac_prefixes settings
    current_ptr = strtok_save(mac_prefixes, DELIMITER_COMMA, &save_ptr);
    new_node->number_mac_prefixes = atoi (current_ptr);
    for(i = 0 ; i < new_node->number_mac_prefixes ; i++){
        temp_value = strtok_save(NULL, DELIMITER_COMMA, &save_ptr);
        strcpy(new_node->mac_prefixes[i], temp_value);
    }

    insert_list_tail( &new_node -> geo_fence_list_entry,
                      geo_fence_list_head);
#ifdef debugging    
    zlog_info(category_debug, "<<add_geo_fence_setting");
#endif
    return WORK_SUCCESSFULLY;
}


ErrorCode check_geo_fence_violations(BufferNode *buffer_node)
{
     /* The format of the tracked object data:
     * lbeacon_uuid;lbeacon_datetime,lbeacon_ip;object_type;object_number;
     * object_mac_address_1;initial_timestamp_GMT_1;final_timestamp_GMT_1;
     * rssi_1;panic_button_1;object_type;object_number;object_mac_address_2;
     * initial_timestamp_GMT_2;final_timestamp_GMT_2;rssi_2;panic_button_2;
     */
    char content_temp[WIFI_MESSAGE_LENGTH];
    char *save_ptr = NULL;

    List_Entry * current_list_entry = NULL;
    GeoFenceListNode *current_list_ptr = NULL;

    char *lbeacon_uuid = NULL;
    char *lbeacon_datetime = NULL;
    char *lbeacon_ip = NULL;
    char *object_type = NULL;
    char *number_of_objects = NULL;
    char *mac_address = NULL;
    char *initial_timestamp = NULL;
    char *final_timestamp = NULL;
    char *rssi = NULL;
    char *panic_button = NULL;

    int i = 0;
    int number_types = 0;
    int number_objects = 0;
    int detected_rssi = 0;

    bool is_perimeter_lbeacon = false;
    bool is_fence_lbeacon = false;

#ifdef debugging
    zlog_info(category_debug, ">>check_geo_fence_violations");
#endif

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    lbeacon_uuid = strtok_save(content_temp, DELIMITER_SEMICOLON, &save_ptr);
    

    list_for_each(current_list_entry, &(config.geo_fence_list_head)){

        current_list_ptr = ListEntry(current_list_entry,
                                     GeoFenceListNode,
                                     geo_fence_list_entry);

        // First, check if lbeacon_uuid is listed as perimeter LBeacon or 
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
#ifdef debugging
           zlog_info(category_debug, 
                     "lbeacon_uuid=[%s] is not in geo-fence setting name=[%s]", 
                     lbeacon_uuid, current_list_ptr->name);
#endif
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
        
            number_of_objects = strtok_save(NULL, 
                                            DELIMITER_SEMICOLON, 
                                            &save_ptr);
            number_objects = atoi(number_of_objects);

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
       
                for(i = 0 ; i < current_list_ptr->number_mac_prefixes ; i++){
                    if(0 == 
                        strncmp_caseinsensitive(
                            mac_address, 
                            current_list_ptr->mac_prefixes[i], 
                            strlen(current_list_ptr->mac_prefixes[i]))){

                        detected_rssi = atoi(rssi);
                
                        if(is_perimeter_lbeacon && 
                           detected_rssi > 
                           current_list_ptr->rssi_of_perimeters){
#ifdef debugging
                            zlog_info(category_debug, 
                                      "[GeoFence-Perimeter]: LBeacon UUID=[%s]" \
                                      "mac_address=[%s]", 
                                      lbeacon_uuid, mac_address);
#endif

                            insert_into_geo_fence_alert_list(
                                mac_address,
                                current_list_ptr->name, 
                                GEO_FENCE_ALERT_TYPE_PERIMETER,
                                lbeacon_uuid,
                                final_timestamp,
                                rssi);
                        }
                        if(is_fence_lbeacon && 
                           detected_rssi > 
                           current_list_ptr->rssi_of_fences){
#ifdef debugging
                            zlog_info(category_debug, 
                                      "[GeoFence-Fence]: LBeacon UUID=[%s] "\
                                      "mac_address=[%s]", 
                                      lbeacon_uuid, mac_address);
#endif

                            insert_into_geo_fence_alert_list(
                                mac_address,
                                current_list_ptr->name, 
                                GEO_FENCE_ALERT_TYPE_FENCE,
                                lbeacon_uuid,
                                final_timestamp,
                                rssi);
                        }
                        break;
                    }
                }
            }
        }
    }

#ifdef debugging
    zlog_info(category_debug, "<<check_geo_fence_violations");
#endif

    return WORK_SUCCESSFULLY;
}

ErrorCode insert_into_geo_fence_alert_list(char *mac_address, 
                                           char *fence_name,
                                           char *fence_type, 
                                           char *lbeacon_uuid, 
                                           char *final_timestamp, 
                                           char *rssi){
        BufferNode *new_node = NULL;

#ifdef debugging
        zlog_info(category_debug, ">> insert_into_geo_fence_alert_list");
#endif

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

#ifdef debugging
        zlog_info(category_debug, "<< insert_into_geo_fence_alert_list");
#endif

        return WORK_SUCCESSFULLY;
 }


