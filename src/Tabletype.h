
#ifndef TABLETYPE_H
#define TABLETYPE_H

#include "BeDIS.h"

#define LENGTH_OF_TIMESTAMP 11
#define LENGTH_OF_PANIC_BUTTON 2
#define LENGTH_OF_BATTERY_VOLTAGE 4
#define SIZE_OF_RSSI_ARRAY 10
#define INITIAL_RECORD_TABLE_SIZE 32
typedef struct {
	
   char* lbeacon_uuid;
   char* initial_timestamp_GMT;
   char* final_timestamp_GMT;
   char* battery_voltage;   
   char* rssi;
   char* panic_button;
   pthread_mutex_t list_lock;
} DataForHashtable;

//Record array

typedef struct {
   char uuid[LENGTH_OF_UUID];
   char initial_timestamp[LENGTH_OF_TIMESTAMP];
   char final_timestamp[LENGTH_OF_TIMESTAMP];
   int head;
   //10 seconds
   int rssi_array[SIZE_OF_RSSI_ARRAY];
   float coordinateX;
   float coordinateY;
   int valid;
} uuid_record_table_row;


typedef struct {
	
   char summary_uuid[LENGTH_OF_UUID];
   char battery[LENGTH_OF_BATTERY_VOLTAGE];
   char initial_timestamp[LENGTH_OF_TIMESTAMP];
   char final_timestamp[LENGTH_OF_TIMESTAMP];
   int average_rssi;
   float summary_coordinateX;
   float summary_coordinateY;  
   size_t record_table_size;
   //upload or not
   int recently_scanned;
   char panic_button[LENGTH_OF_PANIC_BUTTON];
   uuid_record_table_row* uuid_record_table_array;//[INITIAL_RECORD_TABLE_SIZE]
} hash_table_row;



#endif