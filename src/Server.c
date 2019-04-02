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

     1.0, 20190307

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

int main(int argc, char **argv){

    int return_value;

    /* The pkt type to be send */
    int send_pkt_type;

    /* The msg for sending commend */
    char command_msg[MINIMUM_WIFI_MESSAGE_LENGTH];

    /* The command for opening database */
    char database_argument[SQL_TEMP_BUFFER_LENGTH];

    int current_time;

    /* The main thread of the communication Unit */
    pthread_t CommUnit_thread;

    /* The thread to listen for messages from Wi-Fi interface */
    pthread_t wifi_listener;

    /* All global flags */
    NSI_initialization_complete      = false;
    CommUnit_initialization_complete = false;

    /* Initialize flags */

    initialization_failed = false;

    ready_to_work = true;

#ifdef debugging
    printf("Start Server\n");
#endif

    /* Create the config from input config file */

    if(get_config( &config, CONFIG_FILE_NAME) != WORK_SUCCESSFULLY){

        return E_OPEN_FILE;
    }

    printf("Mempool Initializing\n");

    /* Initialize the memory pool */
    printf("Buffer Node [%d]\n", sizeof(BufferNode));
    if(mp_init( &node_mempool, sizeof(BufferNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS){

        return E_MALLOC;
    }

    printf("Mempool Initialized\n");

    printf("Start connect to Database\n");

    /* Open DB */

    memset(database_argument, 0, SQL_TEMP_BUFFER_LENGTH);

    sprintf(database_argument, "dbname=%s user=%s password=%s host=%s port=%d",
                               config.database_name, config.database_account,
                               config.database_password, config.db_ip,
                               config.database_port );

#ifdef debugging
    printf("Database Argument [%s]\n", database_argument);
#endif

    SQL_open_database_connection(database_argument, &Server_db);

    printf("Database connected\n");

    printf("Initialize buffer lists\n");

    /* Initialize the address map*/
    init_Address_Map( &Gateway_address_map);

    /* Initialize buffer_list_heads and add to the head in to the priority list.
     */

    init_buffer( &priority_list_head, (void *) sort_priority_list,
                config.high_priority);

    init_buffer( &time_critical_Gateway_receive_buffer_list_head,
                (void *) Gateway_routine, config.high_priority);
    insert_list_tail( &time_critical_Gateway_receive_buffer_list_head
                     .priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &Gateway_receive_buffer_list_head,
                (void *) Gateway_routine, config.normal_priority);
    insert_list_tail( &Gateway_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &LBeacon_receive_buffer_list_head,
                (void *) LBeacon_routine, config.high_priority);
    insert_list_tail( &LBeacon_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_send_buffer_list_head,
                (void *) process_wifi_send, config.normal_priority);
    insert_list_tail( &NSI_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &NSI_receive_buffer_list_head,
                (void *) NSI_routine, config.normal_priority);
    insert_list_tail( &NSI_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_receive_buffer_list_head,
                (void *) BHM_routine, config.low_priority);
    insert_list_tail( &BHM_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_send_buffer_list_head,
                (void *) process_wifi_send, config.low_priority);
    insert_list_tail( &BHM_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    sort_priority_list( &priority_list_head);

    printf("Buffer lists initialized\n");

    printf("Initialize sockets\n");

    /* Initialize the Wifi connection */
    if(return_value = Wifi_init(config.server_ip) != WORK_SUCCESSFULLY){
        /* Error handling and return */
        initialization_failed = true;
        printf("Fail to initialize sockets\n");
        return E_WIFI_INIT_FAIL;
    }

    return_value = startThread( &wifi_listener, (void *)process_wifi_receive,
                               NULL);

    if(return_value != WORK_SUCCESSFULLY){
        initialization_failed = true;

        return E_WIFI_INIT_FAIL;
    }

    printf("Sockets initialized\n");

    NSI_initialization_complete = true;

    printf("Network Setup and Initialize success\n");

    printf("Initialize Communication Unit\n");

    /* Create the main thread of Communication Unit  */
    return_value = startThread( &CommUnit_thread, CommUnit_routine, NULL);

    if(return_value != WORK_SUCCESSFULLY){

        return return_value;
    }

    printf("Start Communication\n");

    /* The while loop waiting for NSI and CommUnit to be ready */
    while(NSI_initialization_complete == false ||
          CommUnit_initialization_complete == false){

        Sleep(WAITING_TIME);

        if(initialization_failed == true){
            ready_to_work = false;

            /* The program is going to be ended. Free the connection of Wifi */
            Wifi_free();

            SQL_close_database_connection(Server_db);

            return E_INITIALIZATION_FAIL;
        }
    }

    current_time = get_system_time();

    /* The while loop that keeps the program running */
    while(ready_to_work == true){

        current_time = get_system_time();

        /* If it is the time to poll health reports from LBeacons, get a
           thread to do this work */
        if(current_time - last_polling_object_tracking_time >
           config.period_between_RFTOD){

            /* Pull object tracking object data */
            /* set the pkt type */
            send_pkt_type = ((from_server & 0x0f) << 4) +
                             (tracked_object_data &
                             0x0f);
            memset(command_msg, 0, MINIMUM_WIFI_MESSAGE_LENGTH);

            command_msg[0] = (char)send_pkt_type;

//#ifdef debugging
            printf("Send Request for Tracked Object Data\n");
//#endif

            /* broadcast to LBeacons */
            Gateway_Broadcast(&Gateway_address_map, command_msg,
                             MINIMUM_WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_object_tracking_time */
            last_polling_object_tracking_time = current_time;
        }
        else if(current_time - last_polling_LBeacon_for_HR_time >
                config.period_between_RFHR){

            /* Polling for health reports. */
            /* set the pkt type */
            send_pkt_type = ((from_server & 0x0f) << 4) +
                             (health_report & 0x0f);
            memset(command_msg, 0, MINIMUM_WIFI_MESSAGE_LENGTH);

            command_msg[0] = (char)send_pkt_type;

//#ifdef debugging
            printf("Send Request for Health Report\n");
//#endif

            /* broadcast to LBeacons */
            Gateway_Broadcast(&Gateway_address_map, command_msg,
                             MINIMUM_WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_LBeacon_for_HR_time */
            last_polling_LBeacon_for_HR_time = get_system_time();
        }

        Sleep(WAITING_TIME);

    }

    /* The program is going to be ended. Free the connection of Wifi */
    Wifi_free();

    SQL_close_database_connection(Server_db);

    return WORK_SUCCESSFULLY;
}


ErrorCode get_config(ServerConfig *config, char *file_name) {

    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        printf("Load config fail\n");
        return E_OPEN_FILE;
    }
    else {

        /* Create spaces for storing the string in the current line being read*/
        char  config_setting[CONFIG_BUFFER_SIZE];
        char *config_message = NULL;
        int config_message_size = 0;

        /* Keep reading each line and store into the config struct */
        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->server_ip, config_message, config_message_size);
        printf("Server IP [%s]\n", config->server_ip);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->db_ip, config_message, config_message_size);
        printf("Database IP [%s]\n", config->db_ip);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->allowed_number_nodes = atoi(config_message);
        printf("Allow Number of Nodes [%d]\n", config->allowed_number_nodes);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_RFHR = atoi(config_message);
        printf("Periods between request for health report [%d]\n",
               config->period_between_RFHR);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_RFTOD = atoi(config_message);
        printf("Periods between request for tracked object data [%d]\n",
                config->period_between_RFTOD);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->number_worker_threads = atoi(config_message);
        printf("Number of worker threads [%d]\n",
               config->number_worker_threads);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->send_port = atoi(config_message);
        printf("The destination port when sending [%d]\n", config->send_port);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->recv_port = atoi(config_message);
        printf("The received port [%d]\n", config->recv_port);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->database_port = atoi(config_message);
        printf("The database port [%d]\n", config->database_port);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->database_name, config_message, config_message_size);

#ifdef debugging
        printf("Database Name [%s]\n", config->database_name);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->database_account, config_message, config_message_size);

#ifdef debugging
        printf("Database Account [%s]\n", config->database_account);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->database_password, config_message, config_message_size);

#ifdef debugging
        printf("Database Password [%s]\n", config->database_password);
#endif

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->critical_priority = atoi(config_message);
        printf("The nice of critical priority is [%d]\n",
               config->critical_priority);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->high_priority = atoi(config_message);
        printf("The nice of high priority is [%d]\n", config->high_priority);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->normal_priority = atoi(config_message);
        printf("The nice of normal priority is [%d]\n",
               config->normal_priority);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->low_priority = atoi(config_message);
        printf("The nice of low priority is [%d]\n", config->low_priority);

        fclose(file);

    }
    return WORK_SUCCESSFULLY;
}


void init_buffer(BufferListHead *buffer_list_head, void (*function_p)(void *),
                 int priority_nice){

    init_entry( &(buffer_list_head -> list_head));

    init_entry( &(buffer_list_head -> priority_list_entry));

    pthread_mutex_init( &buffer_list_head->list_lock, 0);

    buffer_list_head -> function = function_p;

    buffer_list_head -> arg = (void *) buffer_list_head;

    buffer_list_head -> priority_nice = priority_nice;
}


void *sort_priority_list(BufferListHead *list_head){

    List_Entry *list_pointers,
               *list_pointers_in_bubble,
               *save_list_pointers_in_bubble,
               tmp;

    BufferListHead *current_head, *next_head;

    pthread_mutex_lock( &list_head -> list_lock);

    list_for_each(list_pointers,  &list_head -> priority_list_entry){

        list_for_each_safe_reverse(list_pointers_in_bubble,
                                   save_list_pointers_in_bubble,
                                    &list_head -> priority_list_entry){

            current_head = ListEntry(list_pointers_in_bubble, BufferListHead,
                                     priority_list_entry);
            next_head = ListEntry(save_list_pointers_in_bubble, BufferListHead,
                                  priority_list_entry);

            /* save_list_pointers_in_bubble <-> list_pointers_in_bubble <-> head
             */

            if(current_head -> priority_nice < next_head -> priority_nice){

                tmp.prev = save_list_pointers_in_bubble -> prev;
                tmp.next = list_pointers_in_bubble -> next;

                list_pointers_in_bubble -> prev = tmp.prev;
                list_pointers_in_bubble -> next = save_list_pointers_in_bubble;

                save_list_pointers_in_bubble -> next = tmp.next;
                save_list_pointers_in_bubble -> prev = list_pointers_in_bubble;

                list_pointers_in_bubble -> prev -> next =
                                                        list_pointers_in_bubble;
                save_list_pointers_in_bubble -> next -> prev =
                                                   save_list_pointers_in_bubble;

                save_list_pointers_in_bubble = list_pointers_in_bubble;
            }

            if(list_pointers_in_bubble == list_pointers){
                break;
            }
        }
    }

    pthread_mutex_unlock( &list_head -> list_lock);

    return (void *)NULL;
}


void *CommUnit_routine(){

    /* The last reset time */
    int init_time;

    int current_time;

    Threadpool thpool;

    int return_error_value;

    /* The pointer point to the current priority buffer list entry */
    List_Entry *current_entry, *list_entry;

    BufferNode *current_node;

    /* The pointer point to the current buffer list head */
    BufferListHead *current_head;

    /* wait for NSI get ready */
    while(NSI_initialization_complete == false){
        Sleep(WAITING_TIME);
        if(initialization_failed == true){
            return (void *)NULL;
        }
    }

    /* Initialize the threadpool with specified number of worker threads
       according to the data stored in the config file. */
    thpool = thpool_init(config.number_worker_threads);

    current_time = get_system_time();

    /* Set the initial time. */
    init_time = current_time;

    /* All the buffers lists have been are initialized and the thread pool
       initialized. Set the flag to true. */
    CommUnit_initialization_complete = true;

    /* When there is no dead thead, do the work. */
    while(ready_to_work == true){

        current_time = get_system_time();

        /* In the normal situation, the scanning starts from the high priority
           to lower priority. When the timer expired for MAX_STARVATION_TIME,
           reverse the scanning process */
        while(current_time - init_time < MAX_STARVATION_TIME){

            while(thpool -> num_threads_working == thpool -> num_threads_alive){
                Sleep(WAITING_TIME);
            }

            /* Scan the priority_list to get the buffer list with the highest
               priority among all lists that are not empty. */

            pthread_mutex_lock( &priority_list_head.list_lock);

            list_for_each(current_entry,
                          &priority_list_head.priority_list_entry){

                current_head = ListEntry(current_entry, BufferListHead,
                                         priority_list_entry);

                pthread_mutex_lock( &current_head -> list_lock);

                if (is_entry_list_empty( &current_head->list_head) == true){

                    pthread_mutex_unlock( &current_head -> list_lock);
                    /* Go to check the next buffer list in the priority list */
                    continue;
                }
                else {

                    list_entry = current_head -> list_head.next;

                    remove_list_node(list_entry);

                    pthread_mutex_unlock( &current_head -> list_lock);

                    current_node = ListEntry(list_entry, BufferNode,
                                             buffer_entry);

                    /* If there is a node in the buffer and the buffer is not be
                       occupied, do the work according to the function pointer
                     */
                    return_error_value = thpool_add_work(thpool,
                                                         current_head -> function,
                                                         current_node,
                                                         current_head ->
                                                         priority_nice);

                    break;
                }
            }

            current_time = get_system_time();
            pthread_mutex_unlock( &priority_list_head.list_lock);

			Sleep(WAITING_TIME);
        }

        while(thpool -> num_threads_working == thpool -> num_threads_alive){
            Sleep(WAITING_TIME);
        }

        /* Scan the priority list in reverse order to prevent starving the
           lowest priority buffer list. */

        pthread_mutex_lock( &priority_list_head.list_lock);

        list_for_each_reverse(current_entry,
                              &priority_list_head.priority_list_entry){

            current_head = ListEntry(current_entry, BufferListHead,
                                     priority_list_entry);

            pthread_mutex_lock( &current_head -> list_lock);

            if (is_entry_list_empty( &current_head->list_head) == true){

                pthread_mutex_unlock( &current_head -> list_lock);
                /* Go to check the next buffer list in the priority list */
                continue;
            }
            else {

                list_entry = current_head -> list_head.next;

                remove_list_node(list_entry);

                pthread_mutex_unlock( &current_head -> list_lock);

                current_node = ListEntry(list_entry, BufferNode,
                                         buffer_entry);

                /* If there is a node in the buffer and the buffer is not be
                   occupied, do the work according to the function pointer */
                return_error_value = thpool_add_work(thpool,
                                                     current_head -> function,
                                                     current_node,
                                                     current_head ->
                                                     priority_nice);

                break;
            }
        }

        /* Update the init_time */
        init_time = get_system_time();

        pthread_mutex_unlock( &priority_list_head.list_lock);

		Sleep(WAITING_TIME);

    } /* End while(ready_to_work == true) */


    /* Destroy the thread pool */
    thpool_destroy(thpool);

    return (void *)NULL;
}


void *NSI_routine(void *_buffer_node){

    BufferNode *current_node = (BufferNode *)_buffer_node;

    int send_pkt_type;

    char  *gateway_record = malloc(WIFI_MESSAGE_LENGTH*sizeof(char));

    send_pkt_type = (from_server & 0x0f)<<4;

#ifdef debugging
    printf("Start join...(%s)\n", current_node -> net_address);
#endif

    /* Put the address into Gateway_address_map and set the return pkt type
     */
    if (Gateway_join_request(&Gateway_address_map, current_node ->
                             net_address) == true)
        send_pkt_type += join_request_ack & 0x0f;
    else
        send_pkt_type += join_request_deny & 0x0f;

    memset(gateway_record, 0, WIFI_MESSAGE_LENGTH*sizeof(char));

    sprintf(gateway_record, "1;%s;%d;", current_node -> net_address,
            S_NORMAL_STATUS);

    SQL_update_gateway_registration_status(Server_db, gateway_record,
                                           strlen(gateway_record));

    SQL_update_lbeacon_registration_status(Server_db,
                                           &current_node->content[2],
                                           strlen(&current_node->content[2]));

    /* put the pkt type to content */
    current_node->content[0] = (char)send_pkt_type;

    pthread_mutex_lock(&NSI_send_buffer_list_head.list_lock);

    insert_list_tail( &current_node->buffer_entry,
                      &NSI_send_buffer_list_head.list_head);

    pthread_mutex_unlock( &NSI_send_buffer_list_head.list_lock);

    printf("%s join success\n", current_node -> net_address);

    return (void *)NULL;
}


void *BHM_routine(void *_buffer_node){

    BufferNode *current_node = (BufferNode *)_buffer_node;

    char  *lbeacon_record = malloc(WIFI_MESSAGE_LENGTH * sizeof(char));

    memset(lbeacon_record, 0, WIFI_MESSAGE_LENGTH * sizeof(char));

    sprintf(lbeacon_record, "1;%s;", &current_node ->content[1]);

    SQL_update_lbeacon_health_status(Server_db,
                                     lbeacon_record,
                                     strlen(lbeacon_record));

    mp_free( &node_mempool, current_node);

    return (void *)NULL;
}


void *LBeacon_routine(void *_buffer_node){

    int pkt_type;

    BufferNode *current_node = (BufferNode *)_buffer_node;

    /* read the pkt type from lower lower 4 bits. */
    pkt_type = current_node ->content[0] & 0x0f;

    if(pkt_type == tracked_object_data){
        SQL_update_object_tracking_data(Server_db,
                                        &current_node ->content[1],
                                        strlen(&current_node ->content[1]));
    }

    mp_free( &node_mempool, current_node);

    return (void* )NULL;
}


void *Gateway_routine(void *_buffer_node){

    int pkt_type;

    BufferNode *current_node = (BufferNode *)_buffer_node;

    /* read the pkt type from lower lower 4 bits. */
    pkt_type = current_node ->content[0] & 0x0f;

    if(pkt_type == tracked_object_data){
        SQL_update_object_tracking_data(Server_db,
                                        &current_node ->content[1],
                                        strlen(&current_node ->content[1]));
    }

    mp_free( &node_mempool, current_node);

    return (void* )NULL;
}


void init_Address_Map(AddressMapArray *address_map){

    int n;

    pthread_mutex_init( &address_map -> list_lock, 0);

    memset(address_map -> address_map_list, 0,
           sizeof(address_map -> address_map_list));

    for(n = 0; n < MAX_NUMBER_NODES; n ++)
        address_map -> in_use[n] = false;
}


int is_in_Address_Map(AddressMapArray *address_map, char *net_address){

    int n;

    for(n = 0;n < MAX_NUMBER_NODES;n ++){

        if (address_map -> in_use[n] == true && strncmp(address_map ->
            address_map_list[n].net_address, net_address, NETWORK_ADDR_LENGTH)
            == 0){
                return n;
        }

    }
    return -1;
}


bool Gateway_join_request(AddressMapArray *address_map, char *address){

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

    if(answer=is_in_Address_Map(address_map, address) >=0){
        address_map -> address_map_list[answer].last_request_time =
                                                              get_system_time();
        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        printf("Exist and Return\n");
#endif

        return true;
    }

    for(n = 0 ; n < MAX_NUMBER_NODES ; n ++){
        if(address_map -> in_use[n] == false && not_in_use == -1){
            not_in_use = n;
            break;
        }
    }

#ifdef debugging
    printf("Start join...(%s)\n", address);
#endif

    /* If still has space for the LBeacon to register */
    if (not_in_use != -1){

        AddressMap *tmp =  &address_map -> address_map_list[not_in_use];

        address_map -> in_use[not_in_use] = true;

        strncpy(tmp -> net_address, address, NETWORK_ADDR_LENGTH);

        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        printf("Join Success\n");
#endif

        return true;
    }
    else{
        pthread_mutex_unlock( &address_map -> list_lock);

#ifdef debugging
        printf("Join maximum\n");
#endif
        return false;
    }
}


void Gateway_Broadcast(AddressMapArray *address_map, char *msg, int size){

    /* The counter for for-loop*/
    int current_index;

    pthread_mutex_lock( &address_map -> list_lock);

    if (size <= WIFI_MESSAGE_LENGTH){
        for(current_index = 0;current_index < MAX_NUMBER_NODES;current_index ++)
        {

            if (address_map -> in_use[current_index] == true){
                /* Add the content of tje buffer node to the UDP to be sent to
                   the server */
                udp_addpkt( &udp_config,
                            address_map ->
                            address_map_list[current_index].net_address,
                            msg,
                            size);
            }
        }
    }

    pthread_mutex_unlock( &address_map -> list_lock);
}


ErrorCode Wifi_init(char *IPaddress){

    /* Set the address of server */
    array_copy(IPaddress, udp_config.Local_Address, strlen(IPaddress));

    /* Initialize the Wifi cinfig file */
    if(udp_initial( &udp_config, config.send_port, config.recv_port)
                   != WORK_SUCCESSFULLY){

        /* Error handling TODO */
        return E_WIFI_INIT_FAIL;
    }
    return WORK_SUCCESSFULLY;
}


void Wifi_free(){

    /* Release the Wifi elements and close the connection. */
    udp_release( &udp_config);
    return (void)NULL;
}


void *process_wifi_send(void *_buffer_node){

    BufferNode *current_node = (BufferNode *)_buffer_node;

#ifdef debugging
    printf("Start Send pkt\naddress [%s]\nmsg [%s]\nsize [%d]\n",
                                                    current_node->net_address,
                                                    current_node->content,
													current_node->content_size);
#endif

    /* Add the content of tje buffer node to the UDP to be sent to the
       server */
    udp_addpkt( &udp_config, current_node -> net_address,
               current_node->content, current_node->content_size);

    mp_free( &node_mempool, current_node);

#ifdef debugging
    printf("Send Success\n");
#endif

    return (void *)NULL;
}


void *process_wifi_receive(){

    while (ready_to_work == true) {

        BufferNode *new_node;

        sPkt temppkt = udp_getrecv( &udp_config);

        int test_times;

        char *tmp_addr;

        int pkt_direction;

        int pkt_type;

        if(temppkt.type == UDP){

            /* counting test time for mp_alloc(). */
            test_times = 0;

            /* Allocate memory from node_mempool a buffer node for received data
               and copy the data from Wi-Fi receive queue to the node. */
            do{
                if(test_times == TEST_MALLOC_MAX_NUMBER_TIMES)
                    break;
                else if(test_times != 0)
                    Sleep(1);

                new_node = mp_alloc( &node_mempool);
                test_times ++;

            }while( new_node == NULL);

            if(new_node == NULL){
                /* Alloc memory failed, error handling. */
#ifdef debugging
                printf("No memory allow to alloc...\n");
#endif
            }
            else{

				memset(new_node, 0, sizeof(BufferNode));

                /* Initialize the entry of the buffer node */
                init_entry( &new_node -> buffer_entry);

                /* Copy the content to the buffer_node */
                memcpy(new_node -> content, temppkt.content
                     , temppkt.content_size);

                new_node -> content_size = temppkt.content_size;

                tmp_addr = udp_hex_to_address(temppkt.address);

                memcpy(new_node -> net_address, tmp_addr, NETWORK_ADDR_LENGTH);

                free(tmp_addr);

                /* read the pkt direction from higher 4 bits. */
                pkt_direction = (new_node -> content[0] >> 4) & 0x0f;
                /* read the pkt type from lower lower 4 bits. */
                pkt_type = new_node -> content[0] & 0x0f;

                /* Insert the node to the specified buffer, and release
                   list_lock. */
                switch (pkt_direction) {

                    case from_gateway:

                        switch (pkt_type) {

                            case request_to_join:
//#ifdef debugging
                                printf("Get Join request from Gateway.\n");
//#endif
                                pthread_mutex_lock(&NSI_receive_buffer_list_head
                                                   .list_lock);
                                insert_list_tail(&new_node -> buffer_entry,
                                                 &NSI_receive_buffer_list_head
                                                 .list_head);
                                pthread_mutex_unlock(
                                       &NSI_receive_buffer_list_head.list_lock);
                                break;

                            case tracked_object_data:
//#ifdef debugging
                                printf("Get Tracked Object Data from Gateway\n")
                                ;
//#endif
                                pthread_mutex_lock(
                                   &Gateway_receive_buffer_list_head.list_lock);
                                insert_list_tail( &new_node -> buffer_entry,
                                   &Gateway_receive_buffer_list_head.list_head);
                                pthread_mutex_unlock(
                                   &Gateway_receive_buffer_list_head.list_lock);
                                break;

                            case health_report:
//#ifdef debugging
                                printf("Get Health Report from Gateway\n");
//#endif
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

                    case from_beacon:

                        switch (pkt_type) {

                            case tracked_object_data:
//#ifdef debugging
                                printf("Get Tracked Object Data from LBeacon\n")
                                ;
//#endif
                                pthread_mutex_lock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                                insert_list_tail( &new_node -> buffer_entry,
                                   &LBeacon_receive_buffer_list_head.list_head);
                                pthread_mutex_unlock(
                                   &LBeacon_receive_buffer_list_head.list_lock);
                                break;

                            case health_report:
//#ifdef debugging
                                printf("Get Health Report from LBeacon\n");
//#endif
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
        }
        else if(temppkt.type == NONE){
            /* If there is no packet received, Sleep a short time */
            Sleep(WAITING_TIME);
        }
    }
    return (void *)NULL;
}
