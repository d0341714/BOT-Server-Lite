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

     2.0, 20190119

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


errordesc ErrorDesc [] = {

    {WORK_SUCCESSFULLY, "The code works successfullly"},
    {E_MALLOC, "Error allocating memory"},
    {E_OPEN_FILE, "Error opening file"},
    {E_OPEN_DEVICE, "Error opening the dvice"},
    {E_OPEN_SOCKET, "Error opening socket"},
    {E_SEND_OBEXFTP_CLIENT, "Error opening obexftp client"},
    {E_SEND_CONNECT_DEVICE, "Error connecting to obexftp device"},
    {E_SEND_PUSH_FILE, "Error pushing file to device"},
    {E_SEND_DISCONNECT_CLIENT, "Disconnecting the client"},
    {E_SCAN_SET_HCI_FILTER, "Error setting HCI filter"},
    {E_SCAN_SET_INQUIRY_MODE, "Error settnig inquiry mode"},
    {E_SCAN_START_INQUIRY, "Error starting inquiry"},
    {E_SEND_REQUEST_TIMEOUT, "Sending request timeout"},
    {E_ADVERTISE_STATUS, "LE set advertise returned status"},
    {E_ADVERTISE_MODE, "Error setting advertise mode"},
    {E_SET_BLE_PARAMETER, "Error setting parameters of BLE scanning "},
    {E_BLE_ENABLE, "Error enabling BLE scanning"},
    {E_GET_BLE_SOCKET, "Error getting BLE socket options"},
    {E_START_THREAD, "Error creating thread"},
    {E_INIT_THREAD_POOL, "Error initializing thread pool"},
    {E_LOG_INIT, "Error initializing log file"},
    {E_LOG_GET_CATEGORY, "Error getting log category"},
    {E_EMPTY_FILE, "Empty file"},
    {E_INPUT_PARAMETER , "Error of invalid input parameter"},
    {E_ADD_WORK_THREAD, "Error adding work to the work thread"},
    {MAX_ERROR_CODE, "The element is invalid"},
    {E_INITIALIZATION_FAIL, "The Network or Buffer initialization Fail."},
    {E_WIFI_INIT_FAIL, "Wi-Fi initialization Fail."},
    {E_START_COMMUNICAT_ROUTINE_THREAD, "Start Communocation Thread Fail."},
    {E_START_BHM_ROUTINE_THREAD, "Start BHM THread Fail."},
    {E_START_TRACKING_THREAD, "Start Tracking Thread Fail."}
};


unsigned int *uuid_str_to_data(char *uuid) {
    char conversion[] = "0123456789ABCDEF";
    int uuid_length = strlen(uuid);
    unsigned int *data =
        (unsigned int *)malloc(sizeof(unsigned int) * uuid_length);

    if (data == NULL) {
        /* Error handling */
        perror("Failed to allocate memory");
        return NULL;
    }

    unsigned int *data_pointer = data;
    char *uuid_counter = uuid;

    for (; uuid_counter < uuid + uuid_length;

         data_pointer++, uuid_counter += 2) {
        *data_pointer =
            ((strchr(conversion, toupper(*uuid_counter)) - conversion) * 16) +
            (strchr(conversion, toupper(*(uuid_counter + 1))) - conversion);

    }

    return data;
}


unsigned int twoc(int in, int t) {

    return (in < 0) ? (in + (2 << (t - 1))) : in;
}


void trim_string_tail(char *message) {

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


ErrorCode startThread(pthread_t *thread, void *( *start_routine)(void *),
                      void *arg){

    pthread_attr_t attr;

    if ( pthread_attr_init( &attr) != 0 ||
         pthread_create(thread, &attr, start_routine, arg) != 0 ||
         pthread_detach( *thread)){

          printf("Start Thread Error.\n");
          return E_START_THREAD;
    }

    return WORK_SUCCESSFULLY;

}


int get_system_time() {
    /* A struct that stores the time */
    struct timeb t;

    /* Return value as a long long type */
    int system_time;

    /* Convert time from Epoch to time in milliseconds of a long long type */
    ftime(&t);

    /* millisecond ver. */
    /* system_time = 1000 * (long long)t.time + (long long)t.millitm; */

    /* second ver. */
    system_time = (int)t.time;

    return system_time;
}
