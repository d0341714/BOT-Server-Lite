
#ifndef TABLETYPE_H
#define TABLETYPE_H

typedef struct {	
   
   int valid;
   int send;
   
   //int final_timestamp_GMT
   
} PanicMAC;

typedef struct {
	
   char* lbeacon_uuid;
   char* initial_timestamp_GMT;
   char* final_timestamp_GMT;
   char* battery_voltage;   
   char* rssi;
   
} DataForHashtable;

//Record array

typedef struct {
   int valid;
   char* uuid;
   char* initial_timestamp;
   char* final_timestamp;
   int head;
   //10 seconds
   int rssi_array[10];
   //int average_rssi;
   int weights;
   int coordinateX;
   int coordinateY;
   
} uuid_record_table_row;
/*
typedef struct {
	
   uuid_record_table_row row[16];
   
} uuid_record_table;
*/


typedef struct {
	
   char* summary_uuid;
   char* battery;
   char* initial_timestamp;
   char* final_timestamp;
   int average_rssi;
   //幾何平均
   int summary_coordinateX;
   int summary_coordinateY;
   //uuid_record_table uuid_table;
   //uuid_record_table_row uuid_record_table_ptr0;
   //uuid_record_table_row uuid_table[16];  
   uuid_record_table_row* uuid_record_table_ptr[16];
   size_t record_table_size;
   
} hash_table_row;



#endif