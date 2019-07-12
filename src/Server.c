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

    int current_time;

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

    /* Create the serverconfig from input serverconfig file */

    if(get_server_config( &serverconfig, CONFIG_FILE_NAME) != WORK_SUCCESSFULLY)
    {
        return E_OPEN_FILE;
    }

#ifdef debugging
    printf("Mempool Initializing\n");
#endif

    /* Initialize the memory pool */
    if(mp_init( &node_mempool, sizeof(BufferNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS)
    {
        return E_MALLOC;
    }

#ifdef debugging
    printf("Mempool Initialized\n");
#endif

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
                (void *) process_GeoFence_routine, 
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
                (void *) Server_NSI_routine, serverconfig.normal_priority);
    insert_list_tail( &NSI_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_receive_buffer_list_head,
                (void *) Server_BHM_routine, serverconfig.low_priority);
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

    geo_fence_config.GeoFence_alert_list_node_mempool = &node_mempool;
    geo_fence_config.GeoFence_alert_list_head =
        &Geo_fence_alert_buffer_list_head;

    init_geo_fence(&geo_fence_config);

    last_polling_object_tracking_time = 0;
    last_polling_LBeacon_for_HR_time = 0;

    last_update_geo_fence = 0;

    /* The while loop that keeps the program running */
    while(ready_to_work == true)
    {
        current_time = get_system_time();

        /* If it is the time to poll track object data from LBeacons, 
           get a thread to do this work */
        if(current_time - last_polling_object_tracking_time >=
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
            last_polling_object_tracking_time = current_time;
        }

        /* Since period_between_RFTOD is too frequent, we only allow one type 
           of data to be sent at the same time except for tracked object data. 
         */
        if(current_time - last_polling_LBeacon_for_HR_time >=
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
            last_polling_LBeacon_for_HR_time = get_system_time();
        }
        else if(current_time - last_update_geo_fence > 
                period_between_update_geo_fence)
        {
            current_node = NULL;

            current_node = mp_alloc( &node_mempool);
           
            if(NULL == current_node){
                printf("Server main() mp_alloc failed, abort this data\n");
                continue;
            }

            memset(current_node, 0, sizeof(BufferNode));

            init_entry( &current_node -> buffer_entry);

            memset(content, 0, sizeof(content));
            SQL_get_geo_fence(Server_db, content);

            current_node -> pkt_type = GeoFence_data;    
            current_node -> pkt_direction = from_server;
            
            memcpy(current_node -> content, content, strlen(content));
            
            //current_node->net_address
            //current_node->port

            pthread_mutex_lock( &Geo_fence_receive_buffer_list_head.list_lock);
            insert_list_tail( &current_node -> buffer_entry,
                              &Geo_fence_receive_buffer_list_head.list_head);
            pthread_mutex_unlock( &Geo_fence_receive_buffer_list_head.list_lock);

            last_update_geo_fence = get_system_time();
        }
        else
        {
            Sleep(BUSY_WAITING_TIME);
        }
    }/* End while(ready_to_work == true) */

    release_geo_fence( &geo_fence_config);

    /* Release the Wifi elements and close the connection. */
    udp_release( &udp_config);

    SQL_close_database_connection(Server_db);

    mp_destroy(&node_mempool);

    return WORK_SUCCESSFULLY;
}


ErrorCode get_server_config(ServerConfig *serverconfig, char *file_name) 
{
    FILE *file = fopen(file_name, "r");
    
    if (file == NULL) 
    {
#ifdef debugging
        printf("Load serverconfig fail\n");
#endif
        return E_OPEN_FILE;
    }
    else 
    {
        /* Create spaces for storing the string in the current line being read*/
        char config_setting[CONFIG_BUFFER_SIZE];
        char *config_message = NULL;
        int config_message_size = 0;

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

        fclose(file);

    }
    return WORK_SUCCESSFULLY;
}


void *maintain_database()
{
    printf(">>maintain_database\n");
    while(true == ready_to_work){
        printf("SQL_retain_data with database_keep_days=[%d]\n", serverconfig.database_keep_days); 
        SQL_retain_data(Server_db, serverconfig.database_keep_days * 24);

        printf("SQL_vacuum_database\n");
        SQL_vacuum_database(Server_db);

        // Sleep one day before next check
        Sleep(86400 * 1000);
    }
    printf("<<maintain_database\n");
    return (void *)NULL;
}


void *Server_NSI_routine(void *_buffer_node)
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


void *Server_BHM_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    char lbeacon_record[WIFI_MESSAGE_LENGTH];

    memset(lbeacon_record, 0, sizeof(lbeacon_record));

    sprintf(lbeacon_record, "1;%s;", current_node -> content);

    SQL_update_lbeacon_health_status(Server_db,
                                     lbeacon_record,
                                     strlen(lbeacon_record));

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


void *process_GeoFence_routine(void *_buffer_node)
{
    BufferNode *current_node = (BufferNode *)_buffer_node;

    switch (current_node->pkt_type)
    {
        case tracked_object_data:
            geo_fence_check_tracked_object_data_routine( &geo_fence_config, 
                                                        current_node);
            break;

        case GeoFence_data:
            update_geo_fence( &geo_fence_config, current_node);
            break;

        default:
            break;
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
        for(current_index = 0;current_index < MAX_NUMBER_NODES;current_index ++)
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
    //udp_sendpkt(&udp_config, current_node -> net_address,serverconfig.send_port, current_node -> content, current_node -> content_size);
    udp_sendpkt(&udp_config, current_node);

    mp_free( &node_mempool, current_node);

#ifdef debugging
    printf("Send Success\n");
#endif

    return (void *)NULL;
}


void *process_wifi_receive()
{
    BufferNode *new_node, *forward_node;

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
             printf("process_wifi_receive (new_node) mp_alloc failed, abort this data\n");
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
                        forward_node = NULL;
                        
                        forward_node = mp_alloc( &node_mempool);
                        
                        if(NULL == forward_node){
                            printf("process_wifi_receive (forward_node) mp_alloc failed, abort this data and mp_free new_node\n");
                            mp_free( &node_mempool, new_node);
                            continue;
                        }

                        memset(forward_node, 0, sizeof(BufferNode));

                        init_entry( &forward_node -> buffer_entry);
                        
                        forward_node -> pkt_type = new_node -> pkt_type;
                        forward_node -> pkt_direction = new_node -> pkt_direction;
                        memcpy(forward_node -> net_address, new_node -> net_address, strlen(new_node -> net_address) * sizeof(char));
                        forward_node -> port = new_node -> port;
                        memcpy(forward_node -> content, new_node -> content, strlen(new_node -> content) * sizeof(char));
                        forward_node -> content_size = new_node -> content_size;
                        
                        pthread_mutex_lock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                        insert_list_tail( &new_node -> buffer_entry,
                                   &LBeacon_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                        
                        pthread_mutex_lock(
                                  &Geo_fence_receive_buffer_list_head.list_lock);
                        insert_list_tail( &forward_node -> buffer_entry,
                                  &Geo_fence_receive_buffer_list_head.list_head);
                        pthread_mutex_unlock(
                                  &Geo_fence_receive_buffer_list_head.list_lock);
                       
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