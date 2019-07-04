/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     BeDIS.c

  File Description:

     This file contains code of functions used in both Gateway and LBeacon.

  Version:

     2.0, 20190617

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

     Gary Xiao     , garyh0205@hotmail.com
     Joey Zhou     , joeyzhou5566@gmail.com
     Holly Wang    , hollywang@iis.sinica.edu.tw
     Jake Lee      , jakelee@iis.sinica.edu.tw
     Chun Yu Lai   , chunyu1202@gmail.com

 */

#include "BeDIS.h"


unsigned int twoc(int in, int t) 
{
    return (in < 0) ? (in + (2 << (t - 1))) : in;
}


void init_buffer(BufferListHead *buffer_list_head, void (*function_p)(void *),
                 int priority_nice)
{
    init_entry( &(buffer_list_head -> list_head));

    init_entry( &(buffer_list_head -> priority_list_entry));

    pthread_mutex_init( &buffer_list_head -> list_lock, 0);

    buffer_list_head -> function = function_p;

    buffer_list_head -> arg = (void *) buffer_list_head;

    buffer_list_head -> priority_nice = priority_nice;
}


void init_Address_Map(AddressMapArray *address_map)
{
    int n;

    pthread_mutex_init( &address_map -> list_lock, 0);

    memset(address_map -> address_map_list, 0,
           sizeof(address_map -> address_map_list));

    for(n = 0; n < MAX_NUMBER_NODES; n ++)
        address_map -> in_use[n] = false;
}


int is_in_Address_Map(AddressMapArray *address_map, char *net_address)
{
    int n;

    for(n = 0;n < MAX_NUMBER_NODES;n ++)
    {
        if (address_map -> in_use[n] == true && 
            strncmp(address_map -> address_map_list[n].net_address, 
            net_address, NETWORK_ADDR_LENGTH)== 0)
        {
                return n;
        }
    }
    return -1;
}


int udp_sendpkt(pudp_config udp_config, BufferNode *buffer_node)
{
    int pkt_type;

    char content[WIFI_MESSAGE_LENGTH];

    pkt_type = ((buffer_node->pkt_direction << 4) & 0xf0) + 
               (buffer_node->pkt_type & 0x0f);

    memset(content, 0, WIFI_MESSAGE_LENGTH);

    sprintf(content, "%c%s", (char)pkt_type, buffer_node ->content);

    buffer_node -> content_size =  buffer_node -> content_size + 1;
  
    /* Add the content of the buffer node to the UDP to be sent to the 
       destination */
    udp_addpkt(udp_config, 
               buffer_node -> net_address, 
               buffer_node -> port,
               content,
               buffer_node -> content_size);

    return WORK_SUCCESSFULLY;
}


void trim_string_tail(char *message) 
{
    int idx = 0;

    /* discard the whitespace, newline, carry-return characters at the end */
    if(strlen(message) > 0){

        idx = strlen(message) - 1;
        while(10 == message[idx] ||
                13 == message[idx] ||
                32 == message[idx]){

           message[idx] = '\0';
           idx--;
        }
    }
}


void ctrlc_handler(int stop) { ready_to_work = false; }

int strncmp_caseinsensitive(char const *str_a, char const *str_b, size_t len)
{
    int index = 0;
    int diff = 0;

    for(index = 0; index < len; index++)
    {
        diff = tolower((unsigned char)str_a[index]) - 
               tolower((unsigned char)str_b[index]);
        if(0 != diff)
        {
            return -1;
        }
    }
    return 0;
}

ErrorCode startThread(pthread_t *thread, void *( *start_routine)(void *),
                      void *arg)
{
    pthread_attr_t attr;

    if ( pthread_attr_init( &attr) != 0 ||
         pthread_create(thread, &attr, start_routine, arg) != 0 ||
         pthread_detach( *thread))
    {
          printf("Start Thread Error.\n");
          return E_START_THREAD;
    }

    return WORK_SUCCESSFULLY;

}


int get_system_time()
{
    /* Return value as a long long type */
    int system_time;

    time_t now = time(NULL);

    /* second ver. */
    system_time = (int)now;

    return system_time;
}


char *strtok_save(char *str, char *delim, char **saveptr)
{
    char *tmp;

    if(str == NULL)
    {
        tmp = *saveptr;
    }
    else
    {
        tmp = str;
    }

    if(strncmp(tmp, delim, strlen(delim)) == 0)
    {
        *saveptr += strlen(delim) * sizeof(char);
        return NULL;
    }

    return strtok_s(str, delim, saveptr);
}


int display_time(void)
{
    // variables to store date and time components
    int hours, minutes, seconds, day, month, year;

    // time_t is arithmetic time type
    time_t now;

    // Obtain current time
    // time() returns the current time of the system as a time_t value
    time(&now);

    // Convert to local time format and print to stdout
    printf("%s", ctime(&now));

    return 0;
}