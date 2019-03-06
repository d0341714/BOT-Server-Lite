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

     1.0, 20190117

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
	int send_type;
	char temp[MINIMUM_WIFI_MESSAGE_LENGTH];
	int current_time;

    /* The main thread do the communication Unit */
    pthread_t CommUnit_thread;

    /* The thread to listen for messages from Wi-Fi */
    pthread_t wifi_listener;

    /* Reset all flags */
    NSI_initialization_complete      = false;
    CommUnit_initialization_complete = false;
    BHM_initialization_complete      = true; /* TEMP true for skip BHM check*/

    initialization_failed = false;

    ready_to_work = true;

#ifdef debugging
	printf("Start Server\n");
#endif

    /* Reading the config */

    if(get_config( &config, CONFIG_FILE_NAME) != WORK_SUCCESSFULLY){

        return E_OPEN_FILE;
    }

#ifdef debugging
	printf("Mempool Initializing\n");
#endif

    /* Initialize the memory pool */
    if(mp_init( &node_mempool, sizeof(BufferNode), SLOTS_IN_MEM_POOL)
       != MEMORY_POOL_SUCCESS){

        return E_MALLOC;
    }

#ifdef debugging
	printf("Mempool Initialized\n");

	printf("Start connect to Database\n");
#endif

    /* Open DB */
    SQL_open_database_connection("dbname=botdb user=postgres password=bedis402 host=140.109.22.34 port=5432", &Server_db);

#ifdef debugging
	printf("Database connected\n");
#endif
    
	/* Initialize buffer_list_heads and add to the head in to the priority list.
     */

    init_Address_Map( &Gateway_address_map);

    init_buffer( &priority_list_head, (void *) sort_priority,
                config.high_priority);

    init_buffer( &time_critical_Gateway_receive_buffer_list_head,
                (void *) Gateway_routine, config.normal_priority);
    insert_list_tail( &time_critical_Gateway_receive_buffer_list_head
                     .priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &Gateway_receive_buffer_list_head,
                (void *) Gateway_routine, config.normal_priority);
    insert_list_tail( &Gateway_receive_buffer_list_head.priority_list_entry,
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
                (void *) BHM_routine, config.normal_priority);
    insert_list_tail( &BHM_receive_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    init_buffer( &BHM_send_buffer_list_head,
                (void *) process_wifi_send, config.normal_priority);
    insert_list_tail( &BHM_send_buffer_list_head.priority_list_entry,
                      &priority_list_head.priority_list_entry);

    sort_priority( &priority_list_head);

    /* Initialize the Wifi connection */
    if(return_value = Wifi_init(config.server_ip) != WORK_SUCCESSFULLY){
        /* Error handling and return */
        initialization_failed = true;
        return E_WIFI_INIT_FAIL;
    }

    return_value = startThread( &wifi_listener, (void *)process_wifi_receive,
                               NULL);

    if(return_value != WORK_SUCCESSFULLY){
        initialization_failed = true;

        return E_WIFI_INIT_FAIL;
    }

    NSI_initialization_complete = true;

    /* Create the thread of Communication Unit  */
    return_value = startThread( &CommUnit_thread, CommUnit_routine, NULL);

    if(return_value != WORK_SUCCESSFULLY){

        return return_value;
    }

    /* The while loop waiting for NSI, BHM and CommUnit to be ready */
    while(NSI_initialization_complete == false ||
          CommUnit_initialization_complete == false ||
          BHM_initialization_complete == false){

        Sleep(WAITING_TIME);

        if(initialization_failed == true){
            ready_to_work = false;

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
            send_type = ((from_server & 0x0f) << 4) +
                             (poll_for_tracked_object_data_from_server &
                             0x0f);
            memset(temp, 0, MINIMUM_WIFI_MESSAGE_LENGTH);

            temp[0] = (char)send_type;

            /* broadcast to LBeacons */
            Gateway_Broadcast(&Gateway_address_map, temp,
                             MINIMUM_WIFI_MESSAGE_LENGTH);

            /* Update the last_polling_object_tracking_time */
            last_polling_object_tracking_time = current_time;
        }
        else if(current_time - last_polling_LBeacon_for_HR_time >
                config.period_between_RFHR){

            /* Polling for health reports. */
            /* set the pkt type */
            send_type = ((from_server & 0x0f) << 4) +
                             (RFHR_from_server & 0x0f);
            memset(temp, 0, MINIMUM_WIFI_MESSAGE_LENGTH);

            temp[0] = (char)send_type;

            /* broadcast to LBeacons */
            Gateway_Broadcast(&Gateway_address_map, temp,
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

        /* Error handling */
/*
        zlog_error(category_health_report, "Open config file fail.");
*/
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

		config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        if(config_message[strlen(config_message)-1] == '\n')
            config_message_size = strlen(config_message) - 1;
        else
            config_message_size = strlen(config_message);

        memcpy(config->db_ip, config_message, config_message_size);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->allowed_number_nodes = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_RFHR = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->period_between_RFTOD = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->number_worker_threads = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->send_port = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->recv_port = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->critical_priority = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->high_priority = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->normal_priority = atoi(config_message);

        fgets(config_setting, sizeof(config_setting), file);
        config_message = strstr((char *)config_setting, DELIMITER);
        config_message = config_message + strlen(DELIMITER);
        trim_string_tail(config_message);
        config->low_priority = atoi(config_message);

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


void *sort_priority(BufferListHead *list_head){

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

    int init_time;
    int current_time;
    Threadpool thpool;
    int return_error_value;
	List_Entry *tmp;
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

    /* After all the buffers are initialized and the thread pool initialized,
       set the flag to true. */
    CommUnit_initialization_complete = true;

    /* When there is no dead thead, do the work. */
    while(ready_to_work == true){

        current_time = get_system_time();

        /* In the normal situation, the scanning starts from the high priority
           to lower priority. If the timer expired for MAX_STARVATION_TIME,
           reverse the scanning process */
        while(current_time - init_time < MAX_STARVATION_TIME){

            while(thpool -> num_threads_working == thpool -> num_threads_alive){
                Sleep(WAITING_TIME);
            }

            /* Scan the priority_list to get the buffer list with the highest
               priority among all lists that are not empty. */

            pthread_mutex_lock( &priority_list_head.list_lock);

            list_for_each(tmp, &priority_list_head.priority_list_entry){

                current_head= ListEntry(tmp, BufferListHead,
                                        priority_list_entry);

                pthread_mutex_lock( &current_head -> list_lock);

                if (is_entry_list_empty( &current_head->list_head) == true){

                    pthread_mutex_unlock( &current_head -> list_lock);
                    /* Go to check the next buffer list in the priority list */
                    continue;
                }
                else {

					/* If there is a node in the buffer and the buffer is not be
                       occupied, do the work according to the function pointer */
					return_error_value = thpool_add_work(thpool,
                                                         current_head -> function,
                                                         current_head,
                                                         current_head ->
                                                         priority_nice);

					/* Currently, a work thread is processing this buffer list.
                     */
					pthread_mutex_unlock( &current_head -> list_lock);

                    /* Go to check the next buffer list in the priority
                        list */
                    break;
                }
            }

            Sleep(WAITING_TIME);

            current_time = get_system_time();
			pthread_mutex_unlock( &priority_list_head.list_lock);
		}

        while(thpool -> num_threads_working == thpool -> num_threads_alive){
            Sleep(WAITING_TIME);
        }

        /* Scan the priority list in reverse order to prevent starving the
           lowest priority buffer list. */

        pthread_mutex_lock( &priority_list_head.list_lock);

        list_for_each_reverse(tmp, &priority_list_head.priority_list_entry){

            current_head= ListEntry(tmp, BufferListHead, priority_list_entry);

            pthread_mutex_lock( &current_head -> list_lock);

            if (is_entry_list_empty( &current_head->list_head) == true){

                pthread_mutex_unlock( &current_head -> list_lock);
                /* Go to check the next buffer list in the priority list */
                continue;
            }
            else {
				/* If there is a node in the buffer and the buffer is not be
                   occupied, do the work according to the function pointer */
                return_error_value = thpool_add_work(thpool,
                                                     current_head -> function,
                                                     current_head,
                                                     current_head ->
                                                     priority_nice);
                /* Currently, a work thread is processing this buffer list. */
                pthread_mutex_unlock( &current_head -> list_lock);
                /* Go to check the next buffer list in the priority list */
                break;
            }
        }
            
		Sleep(WAITING_TIME);

        /* Update the init_time */
        init_time = get_system_time();

		pthread_mutex_unlock( &priority_list_head.list_lock);
    } /* End while(ready_to_work == true) */


    /* Destroy the thread pool */
    thpool_destroy(thpool);

    return (void *)NULL;
}


void *NSI_routine(void *_buffer_list_head){

    BufferListHead *buffer_list_head = (BufferListHead *)_buffer_list_head;

    struct List_Entry *temp_list_entry_pointers;

    BufferNode *temp;

	int send_type;

	char  *gateway_record = malloc(WIFI_MESSAGE_LENGTH*sizeof(char));

    pthread_mutex_lock( &buffer_list_head -> list_lock);

    if(is_entry_list_empty( &buffer_list_head -> list_head) == false){

        temp_list_entry_pointers = buffer_list_head -> list_head.next;

        remove_list_node(temp_list_entry_pointers);

        pthread_mutex_unlock( &buffer_list_head -> list_lock);

        temp = ListEntry(temp_list_entry_pointers, BufferNode, buffer_entry);

        send_type = (from_server & 0x0f)<<4;

        /* Put the address into Gateway_address_map and set the return pkt type
         */
        if (Gateway_join_request(&Gateway_address_map, temp ->
                                net_address) == true)
            send_type += join_request_ack & 0x0f;
        else
            send_type += join_request_deny & 0x0f;

		printf("send_type [%d]\n", send_type);

		printf("add return [%d]\n", get_system_time());

		printf("Pre Register\n");

		memset(gateway_record, 0, WIFI_MESSAGE_LENGTH*sizeof(char));

		sprintf(gateway_record, "1;%s;", temp -> net_address);

		printf("%s\nlength: %d\n", gateway_record, strlen(gateway_record));

#ifdef debugging
		printf("Registering Gateway...\n");
#endif
        SQL_update_gateway_registration_status(Server_db, gateway_record,
                                               strlen(gateway_record));
		
		SQL_update_lbeacon_registration_status(Server_db, &temp->content[2], strlen(&temp->content[2]));

#ifdef debugging
		printf("Register Gateway Success\n");
#endif

		/* put the pkt type to content */
        temp->content[0] = (char)send_type;

		printf("NSI msg IP: [%s] msg: [%s]\n", temp->net_address, temp->content);

        pthread_mutex_lock(&NSI_send_buffer_list_head.list_lock);

        insert_list_tail( &temp->buffer_entry,
                          &NSI_send_buffer_list_head.list_head);

        pthread_mutex_unlock( &NSI_send_buffer_list_head.list_lock);
    }
    else
        pthread_mutex_unlock( &buffer_list_head -> list_lock);

    return (void *)NULL;
}


void *BHM_routine(void *_buffer_list_head){

    BufferListHead *buffer_list_head = (BufferListHead *)_buffer_list_head;

    /* Create a temporary node and set as the head */
    struct List_Entry *temp_list_entry_pointers;

    BufferNode *temp;

    pthread_mutex_lock( &buffer_list_head->list_lock);

    if(is_entry_list_empty( &buffer_list_head -> list_head) == false){

        temp_list_entry_pointers = buffer_list_head -> list_head.next;

        remove_list_node(temp_list_entry_pointers);

        pthread_mutex_unlock( &buffer_list_head -> list_lock);

        temp = ListEntry(temp_list_entry_pointers, BufferNode, buffer_entry);

        /* TODO  */

        mp_free( &node_mempool, temp);
    }
    else
        pthread_mutex_unlock( &buffer_list_head -> list_lock);

    return (void *)NULL;
}


void *Gateway_routine(void *_buffer_list_head){

    BufferListHead *buffer_list_head = (BufferListHead *)_buffer_list_head;

	int pkt_type;

    /* Create a temporary node and set as the head */
    struct List_Entry *temp_list_entry_pointers;

    BufferNode *temp;

    pthread_mutex_lock( &buffer_list_head -> list_lock);

    if(is_entry_list_empty( &buffer_list_head -> list_head) == false){

        temp_list_entry_pointers = buffer_list_head -> list_head.next;

        remove_list_node(temp_list_entry_pointers);

        pthread_mutex_unlock( &buffer_list_head -> list_lock);

        temp = ListEntry(temp_list_entry_pointers, BufferNode, buffer_entry);

        /* read the pkt type from lower lower 4 bits. */
        pkt_type = temp ->content[0] & 0x0f;
		
		if(pkt_type == tracked_object_data){
			SQL_update_object_tracking_data(Server_db, &temp ->content[1], strlen(&temp ->content[1]));
		}

        mp_free( &node_mempool, temp);
    }
    else
        pthread_mutex_unlock( &buffer_list_head -> list_lock);

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


bool is_in_Address_Map(AddressMapArray *address_map, char *net_address){

	int n;

    for(n = 0;n < MAX_NUMBER_NODES;n ++){

        if (address_map -> in_use[n] == true && strncmp(address_map ->
            address_map_list[n].net_address, net_address, NETWORK_ADDR_LENGTH)
            == 0){
                return true;
        }
    }
    return false;
}


bool Gateway_join_request(AddressMapArray *address_map, char *address){

    int not_in_use = -1;
	int n;

#ifdef debugging
	printf("Enter Gateway_join_request address [%s]\n", address);
#endif

    pthread_mutex_lock( &address_map -> list_lock);
    /* Copy all the necessary information received from the LBeacon to the
       address map. */

    /* Record the first unused address map location in order to store the new
       joined LBeacon. */

#ifdef debugging
	printf("Check whether joined\n");
#endif

	if(is_in_Address_Map(address_map, address) == true){
        for(n = 0 ; n < MAX_NUMBER_NODES ; n ++){
            if(address_map -> in_use[n] == true && strncmp(address_map ->
               address_map_list[n].net_address, address, NETWORK_ADDR_LENGTH) ==
               0){
                address_map -> address_map_list[n].last_request_time =
                                                              get_system_time();
                break;
            }
        }
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
	printf("Start join...\n");
#endif

    /* If still has space for the LBeacon to register */
    if (not_in_use != -1){

        AddressMap *tmp =  &address_map -> address_map_list[not_in_use];

        address_map -> in_use[not_in_use] = true;

        strncpy(tmp -> net_address, address, NETWORK_ADDR_LENGTH);
        
		pthread_mutex_unlock( &address_map -> list_lock);

		printf("[%s] joined\n", address);

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

	int n;

    pthread_mutex_lock( &address_map -> list_lock);

    if (size <= WIFI_MESSAGE_LENGTH){
        for(n = 0;n < MAX_NUMBER_NODES;n ++){

            if (address_map -> in_use[n] == true){
                /* Add the pkt that to be sent to the server */
                udp_addpkt( &udp_config, address_map -> address_map_list[n]
                            .net_address, msg, size);
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


void *process_wifi_send(void *_buffer_list_head){

    BufferListHead *buffer_list_head = (BufferListHead *)_buffer_list_head;

    struct List_Entry *temp_list_entry_pointers;

    BufferNode *temp;

	printf("Enter send\n");

	pthread_mutex_lock( &buffer_list_head -> list_lock);

    if(is_entry_list_empty( &buffer_list_head -> list_head) == false){

        temp_list_entry_pointers = buffer_list_head -> list_head.next;

        remove_list_node(temp_list_entry_pointers);

        pthread_mutex_unlock( &buffer_list_head -> list_lock);

		printf("send return [%d]\n", get_system_time());

#ifdef debugging
		printf("Start Send pkt\naddress [%s]\nmsg [%d]\n", temp->content, temp->content_size);
#endif

        temp = ListEntry(temp_list_entry_pointers, BufferNode, buffer_entry);

        /* Add the content that to be sent to the server */
        udp_addpkt( &udp_config, temp -> net_address, temp->content,
                   temp->content_size);

        mp_free( &node_mempool, temp);

#ifdef debugging
		printf("Send Success\n");
#endif

	}
    else
        pthread_mutex_unlock( &buffer_list_head -> list_lock);

    return (void *)NULL;
}


void *process_wifi_receive(){

    while (ready_to_work == true) {

        BufferNode *new_node;

        sPkt temppkt = udp_getrecv( &udp_config);

		int test_times;

		int current_time;

		char *tmp_addr;

		int pkt_direction;

		int pkt_type;

        if(temppkt.type == UDP){

            /* counting test time for mp_alloc(). */
            test_times = 0;

            current_time = get_system_time();

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
#ifdef debugging
				printf("Data Start Receiving...\n");
#endif
				/* Initialize the entry of the buffer node */
                init_entry( &new_node -> buffer_entry);

                /* Copy the content to the buffer_node */
                memcpy(new_node -> content, temppkt.content
                     , temppkt.content_size);

                new_node -> content_size = temppkt.content_size;

                tmp_addr = udp_hex_to_address(temppkt.address);

                memcpy(new_node -> net_address, tmp_addr, NETWORK_ADDR_LENGTH);

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
								printf("Get Join pkt\n");
                                pthread_mutex_lock(&NSI_receive_buffer_list_head
                                                   .list_lock);
                                insert_list_tail(&new_node -> buffer_entry,
                                                 &NSI_receive_buffer_list_head
                                                 .list_head);
                                pthread_mutex_unlock(
                                       &NSI_receive_buffer_list_head.list_lock);
                                break;

                            case tracked_object_data:
								printf("Get TOD pkt\n");
                                pthread_mutex_lock(
                                   &Gateway_receive_buffer_list_head.list_lock);
                                insert_list_tail( &new_node -> buffer_entry,
                                   &Gateway_receive_buffer_list_head.list_head);
                                pthread_mutex_unlock(
                                   &Gateway_receive_buffer_list_head.list_lock);
                                break;

                            case health_report:
								printf("Get HR pkt\n");
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
