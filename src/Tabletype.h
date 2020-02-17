
#ifndef TABLETYPE_H
#define TABLETYPE_H

#include "BeDIS.h"

/* Number of characters in the panic button information of tracking data */
#define LENGTH_OF_PANIC_BUTTON 2

/* Number of characters in the battery voltage information of tracking data */
#define LENGTH_OF_BATTERY_VOLTAGE 4

/* Number of rssi signals to be trakced in the memory at the same time for 
each object */
#define NUMBER_OF_RSSI_SIGNAL_UNDER_TRACKING 10

/* Number of Lbeacons to be traked in the memory at the same time for each 
object */
#define NUMBER_OF_LBEACON_UNDER_TRACKING 32

typedef struct {
	
   char lbeacon_uuid[LENGTH_OF_UUID];
   char initial_timestamp_GMT[LENGTH_OF_EPOCH_TIME];
   char final_timestamp_GMT[LENGTH_OF_EPOCH_TIME];
   char battery_voltage[LENGTH_OF_BATTERY_VOLTAGE];   
   int rssi;
   char panic_button[LENGTH_OF_PANIC_BUTTON];

} DataForHashtable;


typedef struct {

   char uuid[LENGTH_OF_UUID];
   char initial_timestamp[LENGTH_OF_EPOCH_TIME];
   char final_timestamp[LENGTH_OF_EPOCH_TIME];
   int head;
   int rssi_array[NUMBER_OF_RSSI_SIGNAL_UNDER_TRACKING];
   float coordinateX;
   float coordinateY;
   int valid;

} uuid_record_table_row;


typedef struct {
	
   char summary_uuid[LENGTH_OF_UUID];
   char battery[LENGTH_OF_BATTERY_VOLTAGE];
   char initial_timestamp[LENGTH_OF_EPOCH_TIME];
   char final_timestamp[LENGTH_OF_EPOCH_TIME];
   int average_rssi;
   float summary_coordinateX;
   float summary_coordinateY;  
   size_t record_table_size;
   int recently_scanned;
   char panic_button[LENGTH_OF_PANIC_BUTTON];
   uuid_record_table_row uuid_record_table_array[NUMBER_OF_LBEACON_UNDER_TRACKING];//

} hash_table_row;

#endif