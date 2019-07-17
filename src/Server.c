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

     Holly Wang     , hollywang@iis.sinica.edu.tw
     Jake Lee       , jakelee@iis.sinica.edu.tw
     Ray Chao       , raychao5566@gmail.com
     Gary Xiao      , garyh0205@hotmail.com
     Chun Yu Lai    , chunyu1202@gmail.com
 */


#include "Server.h"

int main(int argc, char **argv)
{
    int return_value;

    /* The type of the packet sent by the server */
    int send_pkt_type;

    /* The command message to be sent */
    char command_msg[MINIMUM_WIFI_MESSAGE_LENGTH];

    /* The database argument for opening database */
    char database_argument[SQL_TEMP_BUFFER_LENGTH];

    int uptime;

    /* Number of character bytes in the packet content */
    int content_size;

    /* The main thread of the communication Unit */
    pthread_t CommUnit_thread;

    /* The thread of maintenance database */
    pthread_t database_maintenance_thread;

    /* The thread to listen for messages from Wi-Fi interface */
    pthread_t wifi_listener_thread;

    BufferNode *current_node;

    char content[WIFI_MESSAGE_LENGTH];

    /* Initialize flags */
    NSI_initialization_complete      = false;
    CommUnit_initialization_complete = false;
    initialization_failed            = false;
    ready_to_work                    = true;

#ifdef debugging
    printf("Start Server\n");
#endif

#ifdef debugging
    printf("Mempool Initializing\n");
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
    printf("Mempool Initialized\n");
#endif

    /* Create the serverconfig from input serverconfig file */

    if(get_config( &serverconfig, CONFIG_FILE_NAME) != WORK_SUCCESSFULLY)
    {
        return E_OPEN_FILE;
    }

#ifdef debugging
    printf("Start connect to Database\n");
#endif

    /* Open DB */

    memset(database_argument, 0, SQL_TEMP_BUFFER_LENGTH);

    sprintf(database_argument, "dbname=%s user=%s password=%s host=%s port=%d",
                               serverconfig.database_name, 
                               serverconfig.database_account,
                               serverconfig.database_password, 
                               serverconfig.db_ip,
                               serverconfig.database_port );

#ifdef debugging
    printf("Database Argument [%s]\n", database_argument);
#endif

    SQL_open_database_connection(database_argument, &Server_db);

#ifdef debugging
    printf("Database connected\n");
#endif

#ifdef debugging
    printf("Initialize buffer lists\n");
#endif

    /* Initialize the address map*/
    init_Address_Map( &Gateway_address_map);

    /* Initialize buffer_list_heads and add to the head in to the priority list.
     */
    init_buffer( &priority_list_head, (void *) sort_priority_list,
                serverconfig.high_priority);

    init_buffer( &Geo_fence_alert_buffer_list_head,
                (void *) process_GeoFence_alert_routine, 
                serverconfig.time_critical_priority);
    insert_list_tail( &Geo_fence_alert_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &Geo_fence_receive_buffer_list_head,
                (void *) process_tracked_data_from_geofence_gateway, 
                serverconfig.time_critical_priority);
    insert_list_tail( &Geo_fence_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &LBeacon_receive_buffer_list_head,
                (void *) LBeacon_routine, serverconfig.normal_priority);
    insert_list_tail( &LBeacon_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_send_buffer_list_head,
                (void *) process_wifi_send, serverconfig.normal_priority);
    insert_list_tail( &NSI_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_receive_buffer_list_head,
                (void *) NSI_routine, serverconfig.normal_priority);
    insert_list_tail( &NSI_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_receive_buffer_list_head,
                (void *) BHM_routine, serverconfig.low_priority);
    insert_list_tail( &BHM_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_send_buffer_list_head,
                (void *) process_wifi_send, serverconfig.low_priority);
    insert_list_tail( &BHM_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    sort_priority_list(&serverconfig, &priority_list_head);

#ifdef debugging
    printf("Buffer lists initialized\n");
#endif

#ifdef debugging
    printf("Initialize sockets\n");
#endif

    /* Initialize the Wifi connection */
    if(udp_initial( &udp_config, serverconfig.recv_port) != WORK_SUCCESSFULLY){

        /* Error handling and return */
        initialization_failed = true;

#ifdef debugging
        printf("Fail to initialize sockets\n");
#endif

        return E_WIFI_INIT_FAIL;
    }

    return_value = startThread( &wifi_listener_thread, 
                               (void *)process_wifi_receive,
                               NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        initialization_failed = true;
        return E_WIFI_INIT_FAIL;
    }

#ifdef debugging
    printf("Sockets initialized\n");
#endif

    NSI_initialization_complete = true;

#ifdef debugging
    printf("Network Setup and Initialize success\n");
#endif

#ifdef debugging
    printf("Initialize Communication Unit\n");
#endif

    /* Create the main thread of Communication Unit  */
    return_value = startThread( &CommUnit_thread, CommUnit_routine, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        return return_value;
    }

    /* Create thread to main database */
    return_value = startThread( &database_maintenance_thread, maintain_database, NULL);

    if(return_value != WORK_SUCCESSFULLY)
    {
        return return_value;
    }


#ifdef debugging
    printf("Start Communication\n");
#endif

    /* The while loop waiting for CommUnit routine to be ready */
    while(CommUnit_initialization_complete == false)
    {
        Sleep(BUSY_WAITING_TIME);

        if(initialization_failed == true)
        {
            ready_to_work = false;

            /* Release the Wifi elements and close the connection. */
            udp_release( &udp_config);

            SQL_close_database_connection(Server_db);

            return E_INITIALIZATION_FAIL;
        }
    }

    last_polling_object_tracking_time = 0;
    last_polling_LBeacon_for_HR_time = 0;

    last_update_geo_fence = 0;

    /* The while loop that keeps the program running */
    while(ready_to_work == true)
    {
        uptime = clock_gettime();

        /* If it is the time to poll track object data from LBeacons, 
           get a thread to do this work */
        if(uptime - last_polling_object_tracking_time >=
           serverconfig.period_between_RFTOD)
        {
            /* Pull object tracking object data */
            /* set the pkt type */
            send_pkt_type = ((from_server & 0x0f) << 4) +
                             (tracked_object_data &
                             0x0f);
            memset(command_msg, 0, MINIMUM_WIFI_MESSAGE_LENGTH);

            command_msg[0] = (char)send_pkt_type;


#ifdef debugging
            display_time();
            printf("Send Request for Tracked Object Data\n");
#endif

            /* Broadcast poll messenge to gateways */
            Broadcast_to_gateway(&Gateway_address_map, command_msg,
                             MINIMUM_WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_object_tracking_time */
            last_polling_object_tracking_time = uptime;
        }

        /* Since period_between_RFTOD is too frequent, we only allow one type 
           of data to be sent at the same time except for tracked object data. 
         */
        if(uptime - last_polling_LBeacon_for_HR_time >=
                serverconfig.period_between_RFHR)
        {
            /* Polling for health reports. */
            /* set the pkt type */
            send_pkt_type = ((from_server & 0x0f) << 4) +
                             (health_report & 0x0f);
            memset(command_msg, 0, MINIMUM_WIFI_MESSAGE_LENGTH);

            command_msg[0] = (char)send_pkt_type;

#ifdef debugging
            display_time();
            printf("Send Request for Health Report\n");
#endif

            /* broadcast to gateways */
            Broadcast_to_gateway(&Gateway_address_map, command_msg,
                              MINIMUM_WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_LBeacon_for_HR_time */
            last_polling_LBeacon_for_HR_time = uptime;
        }
        else
        {
            Sleep(BUSY_WAITING_TIME);
        }
    }/* End while(ready_to_work == true) */

    /* Release the Wifi elements and close the connection. */
    udp_release( &udp_config);

    SQL_close_database_connection(Server_db);

    mp_destroy(&node_mempool);

    mp_destroy(&geofence_mempool);

    return WORK_SUCCESSFULLY;
}


ErrorCode get_config(ServerConfig *serverconfig, char *file_name) 
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
#ifdef debugging
        printf("Load serverconfig fail\n");
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

        memcpy(serverconfig->server_ip, config_message, config_message_size);

#ifdef debugging
        printf("Server IP [%s]\n", serverconfig->server_ip);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(serverconfig->db_ip, config_message, config_message_size);

#ifdef debugging
        printf("Database IP [%s]\n", serverconfig->db_ip);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->allowed_number_nodes = atoi(config_message);

#ifdef debugging
        printf("Allow Number of Nodes [%d]\n", 
               serverconfig->allowed_number_nodes);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->period_between_RFHR = atoi(config_message);

#ifdef debugging
        printf("Periods between request for health report [%d]\n",
               serverconfig->period_between_RFHR);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->period_between_RFTOD = atoi(config_message);

#ifdef debugging
        printf("Periods between request for tracked object data [%d]\n",
                serverconfig->period_between_RFTOD);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->number_worker_threads = atoi(config_message);

#ifdef debugging
        printf("Number of worker threads [%d]\n",
               serverconfig->number_worker_threads);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->send_port = atoi(config_message);

#ifdef debugging
        printf("The destination port when sending [%d]\n", 
               serverconfig->send_port);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->recv_port = atoi(config_message);

#ifdef debugging
        printf("The received port [%d]\n", serverconfig->recv_port);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->database_port = atoi(config_message);

#ifdef debugging
        printf("The database port [%d]\n", serverconfig->database_port);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(serverconfig->database_name, config_message, config_message_size)
        ;

#ifdef debugging
        printf("Database Name [%s]\n", serverconfig->database_name);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(serverconfig->database_account, config_message, 
               config_message_size);

#ifdef debugging
        printf("Database Account [%s]\n", serverconfig->database_account);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(serverconfig->database_password, config_message, 
               config_message_size);


        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->database_keep_days = atoi(config_message);

#ifdef debugging
        printf("Database database_keep_days [%d]\n", serverconfig->database_keep_days);
#endif
        
        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->time_critical_priority = atoi(config_message);

#ifdef debugging
        printf("The nice of time critical priority is [%d]\n",
               serverconfig->time_critical_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->high_priority = atoi(config_message);

#ifdef debugging
        printf("The nice of high priority is [%d]\n", 
               serverconfig->high_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->normal_priority = atoi(config_message);

#ifdef debugging
        printf("The nice of normal priority is [%d]\n",
               serverconfig->normal_priority);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        serverconfig->low_priority = atoi(config_message);

#ifdef debugging
        printf("The nice of low priority is [%d]\n", serverconfig->low_priority)
        ;
#endif

#ifdef debugging
        printf("Initialize geo-fence list\n");
#endif

       /* Initialize geo-fence list head to store all the geo-fence settings */
       init_entry( &(serverconfig->geo_fence_list_head));

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
        
           add_geo_fence_setting(&(serverconfig->geo_fence_list_head), config_message);
       }

#ifdef debugging
       list_for_each(current_list_entry, &(serverconfig->geo_fence_list_head)){
            current_list_ptr = ListEntry(current_list_entry,
                                         GeoFenceListNode,
                                         geo_fence_list_entry);
            
            printf("name=[%s]\n", current_list_ptr->name);

            printf("[perimeters] count=%d\n", current_list_ptr->number_perimeters);
            for(i = 0 ; i < current_list_ptr->number_perimeters ; i++)
                printf("perimeter=[%s], rssi=[%d]\n", current_list_ptr->perimeters[i], 
                                                      current_list_ptr->rssi_of_perimeters);

            printf("[fences] count=%d\n", current_list_ptr->number_fences);
            for(i = 0 ; i < current_list_ptr->number_fences ; i++)
                printf("fence=[%s], rssi=[%d]\n", current_list_ptr->fences[i], 
                                                  current_list_ptr->rssi_of_fences);

            printf("[mac_prefixes] count=%d\n", current_list_ptr->number_mac_prefixes);
            for(i = 0 ; i < current_list_ptr->number_mac_prefixes ; i++)
                printf("mac_prefix=[%s]\n", current_list_ptr->mac_prefixes[i]);
       }
#endif
       
#ifdef debugging
        printf("geo-fence list initialized\n");
#endif

        fclose(file);

    }
    return WORK_SUCCESSFULLY;
}


void *sort_priority_list(ServerConfig *serverconfig, BufferListHead *list_head)
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

        if(current_head -> priority_nice == serverconfig -> time_critical_priority)

            insert_list_tail( list_pointer, &critical_priority_head);

        else if(current_head -> priority_nice == serverconfig -> high_priority)

            insert_list_tail( list_pointer, &high_priority_head);

        else if(current_head -> priority_nice == serverconfig -> 
                normal_priority)

            insert_list_tail( list_pointer, &normal_priority_head);

        else if(current_head -> priority_nice == serverconfig -> low_priority)

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
    printf(">> add_geo_fence_setting\n");
    printf("GeoFence data=[%s][%d]\n", buf, buf_len); 
#endif

    name = strtok_save(buf, DELIMITER_SEMICOLON, &save_ptr);

    perimeters = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);
    
    fences = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

    mac_prefixes = strtok_save(NULL, DELIMITER_SEMICOLON, &save_ptr);

#ifdef debugging
    printf("name=[%s], perimeters=[%s], fences=[%s], mac_prefixes=[%s]\n", 
           name, perimeters, fences, mac_prefixes);
#endif

    new_node = mp_alloc( &geofence_mempool);
           
    if(NULL == new_node){
        printf("[add_geo_fence_setting] mp_alloc failed, abort this data\n");
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
    printf("<<add_geo_fence_setting\n");
#endif
    return WORK_SUCCESSFULLY;
}

void *CommUnit_routine()
{
    /* The last reset time */
    int init_time;

    int uptime;

    Threadpool thpool;

    int return_error_value;

    /* A flag to indicate whether any buffer nodes were processed in 
    this iteration of the while loop */
    bool did_work;

    /* The pointer point to the current priority buffer list entry */
    List_Entry *current_entry, *list_entry;

    BufferNode *current_node;

    /* The pointer point to the current buffer list head */
    BufferListHead *current_head;

    /* wait for NSI get ready */
    while(NSI_initialization_complete == false)
    {
        Sleep(BUSY_WAITING_TIME);
        if(initialization_failed == true)
        {
            return (void *)NULL;
        }
    }
#ifdef debugging
    printf("[CommUnit] thread pool Initializing\n");
#endif
    /* Initialize the threadpool with specified number of worker threads
       according to the data stored in the serverconfig file. */
    thpool = thpool_init(serverconfig.number_worker_threads);

#ifdef debugging
    printf("[CommUnit] thread pool Initialized\n");
#endif

    uptime = clock_gettime();

    /* Set the initial time. */
    init_time = uptime;

    /* All the buffers lists have been are initialized and the thread pool
       initialized. Set the flag to true. */
    CommUnit_initialization_complete = true;

    /* When there is no dead thead, do the work. */
    while(ready_to_work == true)
    {
        did_work = false;

        uptime = clock_gettime();

        /* In the normal situation, the scanning starts from the high priority
           to lower priority. When the timer expired for MAX_STARVATION_TIME,
           reverse the scanning process */
        while(uptime - init_time < MAX_STARVATION_TIME)
        {
            /* Scan the priority_list to get the buffer list with the highest
               priority among all lists that are not empty. */

            pthread_mutex_lock( &priority_list_head.list_lock);

            list_for_each(current_entry,
                          &priority_list_head.priority_list_entry)
            {
                current_head = ListEntry(current_entry, BufferListHead,
                                         priority_list_entry);

                pthread_mutex_lock( &current_head -> list_lock);

                if (is_entry_list_empty( &current_head->list_head) == true)
                {
                    pthread_mutex_unlock( &current_head -> list_lock);
                    /* Go to check the next buffer list in the priority list */

                    continue;
                }
                else 
                {
                    list_entry = current_head -> list_head.next;

                    remove_list_node(list_entry);

                    pthread_mutex_unlock( &current_head -> list_lock);

                    current_node = ListEntry(list_entry, BufferNode,
                                             buffer_entry);

                    /* Call the function specified by the function pointer to 
                       the work */
                    return_error_value = thpool_add_work(thpool,
                                                     current_head -> function,
                                                     current_node,
                                                     current_head ->
                                                     priority_nice);
                    did_work = true;
                    break;
                }
            }
            uptime = clock_gettime();
            pthread_mutex_unlock( &priority_list_head.list_lock);
        }

        /* Scan the priority list in reverse order to prevent starving the
           lowest priority buffer list. */

        pthread_mutex_lock( &priority_list_head.list_lock);

        
        // In starvation scenario, we still need to process time-critial buffer 
        // lists first. We first process Geo_fence_alert_buffer_list_head, 
        // then Geo_fence_receive_buffer_list_head, and finally reversely 
        // traverse the priority list.
        list_for_each(current_entry, &priority_list_head.priority_list_entry)
        {
            current_head = ListEntry(current_entry, BufferListHead,
                                     priority_list_entry);

            if(current_head -> priority_nice == 
                serverconfig.time_critical_priority){

                pthread_mutex_lock( &current_head -> list_lock);

                if (is_entry_list_empty( &current_head->list_head) == true)
                {
                    pthread_mutex_unlock( &current_head -> list_lock);

                    continue;
                }
                else 
                {
                    list_entry = current_head -> list_head.next;

                    remove_list_node(list_entry);

                    pthread_mutex_unlock( &current_head -> list_lock);

                    current_node = ListEntry(list_entry, BufferNode,
                                             buffer_entry);

                    return_error_value = thpool_add_work(thpool,
                                                     current_head -> function,
                                                     current_node,
                                                     current_head ->
                                                     priority_nice);
                    did_work = true;
                    break;
                }
            }
        }

        // reversly traverse the priority list
        list_for_each_reverse(current_entry,
                              &priority_list_head.priority_list_entry)
        {
            current_head = ListEntry(current_entry, BufferListHead,
                                     priority_list_entry);

            pthread_mutex_lock( &current_head -> list_lock);

            if (is_entry_list_empty( &current_head->list_head) == true)
            {
                pthread_mutex_unlock( &current_head -> list_lock);

                continue;
            }
            else 
            {
                list_entry = current_head -> list_head.next;

                remove_list_node(list_entry);

                pthread_mutex_unlock( &current_head -> list_lock);

                current_node = ListEntry(list_entry, BufferNode,
                                         buffer_entry);

                /* Call the function pointed to by the function pointer to do 
                   the work */
                return_error_value = thpool_add_work(thpool,
                                                     current_head -> function,
                                                     current_node,
                                                     current_head ->
                                                     priority_nice);
                did_work = true;
                break;
            }
        }

        /* Update the init_time */
        init_time = clock_gettime();

        pthread_mutex_unlock( &priority_list_head.list_lock);

        /* If during this iteration of while loop no work were done, 
           sleep before starting the next iteration */
        if(did_work == false)
        {
            Sleep(BUSY_WAITING_TIME);
        }

    } /* End while(ready_to_work == true) */

    
    /* Destroy the thread pool */
    thpool_destroy(thpool);

    return (void *)NULL;
}

void *maintain_database()
{
    while(true == ready_to_work){
        printf("SQL_retain_data with database_keep_days=[%d]\n", 
               serverconfig.database_keep_days); 
        SQL_retain_data(Server_db, serverconfig.database_keep_days * 24);

        printf("SQL_vacuum_database\n");
        SQL_vacuum_database(Server_db);

        // Sleep one day before next check
        Sleep(86400 * 1000);
    }
    return (void *)NULL;
}


void *NSI_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    char gateway_record[WIFI_MESSAGE_LENGTH];

    current_node -> pkt_direction = from_server;

#ifdef debugging
    printf("Start join...(%s)\n", current_node -> net_address);
#endif

    /* Put the address into Gateway_address_map and set the return pkt type
     */
    if (Gateway_join_request(&Gateway_address_map, current_node ->
                             net_address) == true)
        current_node -> pkt_type = join_request_ack;
    else
        current_node -> pkt_type = join_request_deny;

    memset(gateway_record, 0, sizeof(gateway_record));

    sprintf(gateway_record, "1;%s;%d;", current_node -> net_address,
            S_NORMAL_STATUS);

    SQL_update_gateway_registration_status(Server_db, gateway_record,
                                           strlen(gateway_record));

    SQL_update_lbeacon_registration_status(Server_db,
                                           &current_node->content[1],
                                           strlen(&current_node->content[1]));

    pthread_mutex_lock(&NSI_send_buffer_list_head.list_lock);

    insert_list_tail( &current_node -> buffer_entry,
                      &NSI_send_buffer_list_head.list_head);

    pthread_mutex_unlock( &NSI_send_buffer_list_head.list_lock);

#ifdef debugging
    printf("%s join success\n", current_node -> net_address);
#endif
    
    return (void *)NULL;
}


void *BHM_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    if(current_node->pkt_direction == from_gateway){

        SQL_update_gateway_health_status(Server_db,
                                         current_node -> content,
                                         current_node -> content_size);
    
    }else if(current_node->pkt_direction == from_beacon){

        SQL_update_lbeacon_health_status(Server_db,
                                         current_node -> content,
                                         current_node -> content_size);

    }

    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}

void *LBeacon_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    if(current_node -> pkt_type == tracked_object_data)
    {
        SQL_update_object_tracking_data(Server_db,
                                        current_node -> content,
                                        strlen(current_node -> content));

    }

    mp_free( &node_mempool, current_node);

    return (void* )NULL;
}


void *process_tracked_data_from_geofence_gateway(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    if(current_node -> pkt_type == tracked_object_data){
        
        check_geo_fence_violations(current_node);

        SQL_update_object_tracking_data(Server_db,
                                        current_node -> content,
                                        strlen(current_node -> content));
        
    }
  
    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}


void *process_GeoFence_alert_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

#ifdef debugging
    printf("Process GeoFence alert [%s]\n", current_node -> content);
#endif

    SQL_insert_geo_fence_alert(Server_db, current_node -> content, 
                               current_node -> content_size);
    
    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}



bool Gateway_join_request(AddressMapArray *address_map, char *address)
{
    int not_in_use = -1;
    int n;
    int answer;

#ifdef debugging
    printf("Enter Gateway_join_request address [%s]\n", address);
#endif

    pthread_mutex_lock( &address_map -> list_lock);
    /* Copy all the necessary information received from the LBeacon to the
       address map. */

    /* Find the first unused address map location and use the location to store
       the new joined LBeacon. */

#ifdef debugging
    printf("Check whether joined\n");
#endif

    if(answer = is_in_Address_Map(address_map, address) >=0)
    {
        strncpy(address_map -> address_map_list[answer].net_address,
                address, NETWORK_ADDR_LENGTH);

        address_map -> address_map_list[answer].last_request_time =
            get_system_time();

        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        printf("Exist and Return\n");
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
    printf("Start join...(%s)\n", address);
#endif

    /* If still has space for the LBeacon to register */
    if (not_in_use != -1)
    {
        AddressMap *tmp =  &address_map -> address_map_list[not_in_use];

        address_map -> in_use[not_in_use] = true;

        strncpy(tmp -> net_address, address, NETWORK_ADDR_LENGTH);

        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        printf("Join Success\n");
#endif

        return true;
    }
    else
    {
        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        printf("Join maximum\n");
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
                            serverconfig.send_port,
                            msg,
                            size);
            }
        }
    }

    pthread_mutex_unlock( &address_map -> list_lock);
}


void *process_wifi_send(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

#ifdef debugging
    printf("Start Send pkt\naddress [%s]\nport [%d]\nmsg [%s]\nsize [%d]\n",
                                                    current_node->net_address,
                                                    serverconfig.send_port,
                                                    current_node->content,
                                                    current_node->content_size);
#endif

    /* Add the content of the buffer node to the UDP to be sent to the
       server */
    //udp_sendpkt(&udp_config, 
    //            current_node -> net_address,serverconfig.send_port, 
    //            current_node -> content, current_node -> content_size);
    udp_sendpkt(&udp_config, current_node);

    mp_free( &node_mempool, current_node);

#ifdef debugging
    printf("Send Success\n");
#endif

    return (void *)NULL;
}


void *process_wifi_receive()
{
    BufferNode *new_node;

    int test_times;

    sPkt temppkt;

    while (ready_to_work == true)
    {
        temppkt = udp_getrecv( &udp_config);

        /* If there is no pkt received */
        if(temppkt.is_null == true)
        {
            Sleep(BUSY_WAITING_TIME);
            continue;
        }

        /* Allocate memory from node_mempool a buffer node for received data
           and copy the data from Wi-Fi receive queue to the node. */
        new_node = NULL;

        new_node = mp_alloc( &node_mempool);
        
        if(NULL == new_node){
             printf("process_wifi_receive (new_node) mp_alloc failed, " \
                    "abort this data\n");
             continue;
        }

        memset(new_node, 0, sizeof(BufferNode));

        /* Initialize the entry of the buffer node */
        init_entry( &new_node -> buffer_entry);

        /* Copy the content to the buffer_node */
        memcpy(new_node -> content, &temppkt.content[1], 
               temppkt.content_size - 1);

        new_node -> content_size = temppkt.content_size - 1;

        new_node -> port = temppkt.port;

        memcpy(new_node -> net_address, temppkt.address,    
               NETWORK_ADDR_LENGTH);

        /* read the pkt direction from higher 4 bits. */
        new_node -> pkt_direction = (temppkt.content[0] >> 4) & 0x0f;
        /* read the pkt type from lower lower 4 bits. */
        new_node -> pkt_type = temppkt.content[0] & 0x0f;

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
                        printf("Get Join request from Gateway.\n");
#endif
                        pthread_mutex_lock( 
                                       &NSI_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                       &NSI_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                       &NSI_receive_buffer_list_head.list_lock);
                        break;

                    case tracked_object_data:
#ifdef debugging
                        display_time();
                        printf("Get tracked object data from Geo_fence Gateway\n");
#endif
                        pthread_mutex_lock(
                                  &Geo_fence_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                  &Geo_fence_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                  &Geo_fence_receive_buffer_list_head.list_lock);

                        break;

                    case health_report:
#ifdef debugging
                        display_time();
                        printf("Get Health Report from Gateway\n");
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

            case from_beacon:

                switch (new_node -> pkt_type) 
                {
                    case tracked_object_data:
#ifdef debugging
                        display_time();
                        printf("Get Tracked Object Data from LBeacon\n");
#endif            
                        pthread_mutex_lock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                   &LBeacon_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                        
                        break;

                    case health_report:
#ifdef debugging
                        display_time();
                        printf("Get Health Report from LBeacon\n");
#endif
                        pthread_mutex_lock(&BHM_receive_buffer_list_head
                                                   .list_lock);
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
    printf(">>check_geo_fence_violations\n");
#endif

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_temp, buffer_node -> content, buffer_node -> content_size);

    lbeacon_uuid = strtok_save(content_temp, DELIMITER_SEMICOLON, &save_ptr);
    

    list_for_each(current_list_entry, &(serverconfig.geo_fence_list_head)){

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
            printf("lbeacon_uuid=[%s] is not in geo-fence setting name=[%s]\n", 
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
                            printf("[GeoFence-Perimeter]: LBeacon UUID=[%s]" \
                                   "mac_address=[%s]\n", 
                                   lbeacon_uuid, 
                                   mac_address);
#endif

                            insert_into_geo_fence_alert_list(mac_address,
                                                             "perimeter",
                                                             lbeacon_uuid,
                                                             final_timestamp,
                                                             rssi);
                        }
                        if(is_fence_lbeacon && 
                           detected_rssi > 
                           current_list_ptr->rssi_of_fences){
#ifdef debugging
                            printf("[GeoFence-Fence]: LBeacon UUID=[%s] "\
                                   "mac_address=[%s]\n", 
                                   lbeacon_uuid, 
                                   mac_address);
#endif

                            insert_into_geo_fence_alert_list(mac_address,
                                                             "fence",
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
    printf("<<check_geo_fence_violations\n");
#endif

    return WORK_SUCCESSFULLY;
}

ErrorCode insert_into_geo_fence_alert_list(char *mac_address, 
                                           char *fence_type, 
                                           char *lbeacon_uuid, 
                                           char *final_timestamp, 
                                           char *rssi){
        BufferNode *new_node = NULL;

#ifdef debugging
        printf(">> insert_into_geo_fence_alert_list\n");
#endif

        new_node = mp_alloc( &node_mempool);
        if(NULL == new_node){
            printf("[insert_into_geo_fence_alert_list] mp_alloc failed, abort "\
                   "this data\n");
            return E_MALLOC;
        }

        memset(new_node, 0, sizeof(BufferNode));

        init_entry( &new_node -> buffer_entry);

        sprintf(new_node -> content, "%d;%s;%s;%s;%s;%s;", 1, 
                                                           mac_address, 
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
        printf("<< insert_into_geo_fence_alert_list\n");
#endif

        return WORK_SUCCESSFULLY;
 }


