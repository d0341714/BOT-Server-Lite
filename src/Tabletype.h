/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Tabletype.h

  File Description:

     This file contains programs to transmit and receive data to and from
     gateways through Wi-Fi network, and programs executed by network setup and
     initialization, beacon health monitor and comminication unit.

  Version:

     1.0, 20200218

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

     Helen Huang    , helenhuang5566@gmail.com 
     Chun-Yu Lai    , chunyu1202@gmail.com

 */

#ifndef TABLETYPE_H
#define TABLETYPE_H

#include "BeDIS.h"

/* Number of characters in the panic button information of tracking data */
#define LENGTH_OF_PANIC_BUTTON 2

/* Number of characters in the battery voltage information of tracking data */
#define LENGTH_OF_BATTERY_VOLTAGE 4

/* Number of rssi signals to be trakced in the memory at the same time for 
each object */
#define MAX_NUMBER_OF_RSSI_SIGNAL_UNDER_TRACKING 32

/* Maximum number of Lbeacons to be traked in the memory at the same time for each 
object */
#define MAX_NUMBER_OF_LBEACON_UNDER_TRACKING 32


/* Structure to store information parsed from tracking data sent by 
lbeacon */
typedef struct {
    
   char lbeacon_uuid[LENGTH_OF_UUID];
   char initial_timestamp_GMT[LENGTH_OF_EPOCH_TIME];
   char final_timestamp_GMT[LENGTH_OF_EPOCH_TIME];
   char battery_voltage[LENGTH_OF_BATTERY_VOLTAGE];   
   int rssi;
   char panic_button[LENGTH_OF_PANIC_BUTTON];

} DataForHashtable;

/* Structure used as member of the hash_table_row structure to store tracking 
information from one lbeacon against the specific mac_address */
typedef struct {

   char uuid[LENGTH_OF_UUID];
   char initial_timestamp[LENGTH_OF_EPOCH_TIME];
   char final_timestamp[LENGTH_OF_EPOCH_TIME];
   int head;
   int rssi_array[MAX_NUMBER_OF_RSSI_SIGNAL_UNDER_TRACKING];
   float coordinateX;
   float coordinateY;

   /* A flag indicating whether this struct is occupied and used to record
   recently scanned data currently. If not, the system will reuse this 
   structure to record data from another lbeacon uuid. */
   bool is_in_use;

} uuid_record_table_row;

/* Structure used as node in hashtable to store all recently tracking 
information from all lbeacons against the specific mac_address */
typedef struct {
    
   char summary_uuid[LENGTH_OF_UUID];

   char initial_timestamp[LENGTH_OF_EPOCH_TIME];
   char final_timestamp[LENGTH_OF_EPOCH_TIME];
   int average_rssi;
   char battery[LENGTH_OF_BATTERY_VOLTAGE];
   char panic_button[LENGTH_OF_PANIC_BUTTON];  
   float summary_coordinateX;
   float summary_coordinateY;  

   int recently_scanned;

   /* Array of uuid_record_table_row to store the lbeacons recently scanning
   this mac_address */
   uuid_record_table_row 
       uuid_record_table_array[MAX_NUMBER_OF_LBEACON_UNDER_TRACKING];

   /* A variable indicating the number of elements in uuid_record_table_array 
   will be used. The value of this variable is defined by server configuration
   file. */
   size_t record_table_size;

} hash_table_row;

#endif