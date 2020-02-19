/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     HashTable.h

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

#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <pthread.h>
#include <string.h>

#include "Tabletype.h"
#include "BeDIS.h"
#include "SqlWrapper.h"
#include <math.h>

/* The prefix of file path of the dumped latest location information */
#define FILE_PREFIX_DUMP_LATEST_LOCATION_INFORMATION "./temp/track"

/* The prefix of file path of the dumped location history information */
#define FILE_PREFIX_DUMP_LOCATION_HISTORY_INFORMATION "./temp/locationtrack"

#define SLOTS_IN_MEM_POOL_HASH_TABLE_NODE 65536

#define SLOTS_IN_MEM_POOL_MAC_ADDRESS 65536

#define INITIAL_AVERAGE_RSSI -100

#define INITIAL_AREA_TABLE_MAX_SIZE 256

#define LENGTH_OF_AREA_ID 5

/* Number of charactures in the time format of %Y-%m-%d %H:%M:%S */
#define LENGTH_OF_TIME_FORMAT 80

/* Type of location information. */
typedef enum _LocationInfoType {

    LATEST_LOCATION_INFO = 0,
    LOCATION_FOR_HISTORY = 1,

} LocationInfoType;

Memory_Pool hash_table_row_mempool;
Memory_Pool mac_address_mempool;

typedef uint32_t (* HashFunc)(const void *, size_t);
typedef int (* EqualityFunc)(void *, void *);
typedef void (* IteratorCallback)(void *, size_t, void *, size_t);

/* Generic helper function used in member of HNode structure */
typedef void (* DeleteData)(void *);

/* Generic node in the hash table */
typedef struct HNode_{
    void * key;
    size_t key_len;
    void * value;
    size_t value_len;
    DeleteData deleteKey;
    DeleteData deleteValue;
    struct HNode_ * next;
} HNode;

typedef struct HashTable {

    HNode ** table;
    int size;
    int count;
    double max_load;
    double resize_factor;
    EqualityFunc equal;
    HashFunc hash;
    DeleteData deleteKey;
    DeleteData deleteValue;
    pthread_mutex_t * ht_mutex;

} HashTable;

typedef struct AreaTable{   

   int area_id;
   HashTable * area_hash_ptr;

} AreaTable;

//dynamic
AreaTable* area_table;
pthread_mutex_t area_table_lock;
int area_table_max_size;
int area_table_size;

/* Designated initializer. Allows for control over max_load percentage, 
resize_factor, and init_size. */
HashTable * hashtable_new(

    int init_size,
    double max_load,
    double resize_factor,
    EqualityFunc equal,
    HashFunc hash,
    DeleteData deleteKey,
    DeleteData deleteValue

);

/* Convenience initializer. Uses default values for initial size, max load 
percentage, and resize factor. */
HashTable * hashtable_new_default(

    EqualityFunc equal,
    DeleteData deleteKey,
    DeleteData deleteValue

);

/* Checks whether an entry with given key exists in hashtable. */
int hashtable_key_exists(HashTable * h_table, void * key, size_t key_len);

int equal_string(void * a, void * b);

void destroy_nop(void * a);

// Function for server
void initial_area_table(int rssi_weight_parameter, int drift_distance, int drift_rssi);

HashTable * hash_table_of_specific_area_id(int area_id);

ErrorCode hashtable_update_object_tracking_data(DBConnectionListHead *db_connection_list_head,
                                                char* buf, 
                                                size_t buf_len, 
                                                int number_of_lbeacons_under_tracked,
                                                int number_of_rssi_signals_under_tracked);

void hashtable_put_mac_table(HashTable * h_table, 
                             void * key, 
                             size_t key_len, 
                             DataForHashtable * value, 
                             int number_of_lbeacons_under_tracked,
                             int number_of_rssi_signals_under_tracked);

/*
  get_rssi_weight:

     This function returns the weight of different rssi singal strength ranges.

  Parameters:

     average_rssi - the average rssi signal strength
         
     rssi_weight_multiplier - the multiplier used to applied on different range 
                              of rssi singal strength

  Return value:

     Corresponding weight of the input averaga rssi
 */

int get_rssi_weight(float average_rssi,
                    int rssi_weight_multiplier);

/*
  hashtable_summarize_location_information:

     This function determines the lbeacon uuid closest to objects and 
     calculates the estimated coordinate_x and coordinate_y of 
     objects.

  Parameters:

     rssi - the 

     unreasonable_rssi_change - the abnormal rssi signal strength change in 
                                adjacent seconds. When this happens, the rssi
                                singal will be ingored in the calculation.

     rssi_weight_multiplier - the multiplier used to applied on different range 
                              of rssi singal strength

     rssi_difference_of_location_accuracy_tolerance - 
         the tolerance of rssi signal to avoid jumping lbecaons

     drift_distance - the tolerance of distance in the calculated results of 
                      base_x and base_y to avoid moving tags when the tags are 
                      not moved.

  Return value:

     None
 */

void hashtable_summarize_location_information(
    HashTable * h_table,
    int number_of_rssi_signals_under_tracked,
    int unreasonable_rssi_change,
    int rssi_weight_multiplier,
    int rssi_difference_of_location_accuracy_tolerance,
    int drift_distance);

/*
  hashtable_traverse_all_areas_to_upload_latest_location:

     This function traverses hashtable of all covered areas to
     upload current location information of objects to current summary 
     location table in the database.

  Parameters:

     db_connection_list_head - the list head of database connection pool

     server_installation_path - the installation of server

     number_of_rssi_signals_under_tracked - 
         the number of rssi signals which are kept in hashtable to calculate
         location of objects. This setting is configurable in server.conf

     unreasonable_rssi_change - the abnormal rssi signal strength change in 
                                adjacent seconds. When this happens, the rssi
                                singal will be ingored in the calculation.

     rssi_weight_multiplier - the multiplier used to applied on different range 
                              of rssi singal strength

     rssi_difference_of_location_accuracy_tolerance - 
         the tolerance of rssi signal to avoid jumping lbecaons

     drift_distance - the tolerance of distance in the calculated results of 
                      base_x and base_y to avoid moving tags when the tags are 
                      not moved.

  Return value:

     None
 */

void hashtable_traverse_all_areas_to_upload_latest_location(
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path,
    int number_of_rssi_signals_under_tracked,
    int unreasonable_rssi_change,
    int rssi_weight_multiplier,
    int rssi_difference_of_location_accuracy_tolerance,
    int drift_distance);

/*
  hashtable_traverse_all_areas_to_upload_history_data:

     This function traverses hashtable of all covered areas to
     upload current location information of objects to history table
     in the database.

  Parameters:

     db_connection_list_head - the list head of database connection pool

     server_installation_path - the installation of server

     number_of_rssi_signals_under_tracked -
         the time length in seconds used to determine whether the last reported
         timestamp of objects are valid and should be treated as existing in 
         the covered area

  Return value:

     None

 */

void hashtable_traverse_all_areas_to_upload_history_data(
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path,
    int number_of_rssi_signals_under_tracked);

/*
  hashtable_upload_location_to_database:

     This function travers all nodes in the input specific hashtable to upload 
     the location information all objects to database. The LocationInfoType is
     used to specified the destination of database tables.

  Parameters:

     h_table - the pointer to specific hashtable of one covered area

     db_connection_list_head - the list head of database connection pool

     server_installation_path - the installation of server

     location_type - the destination of database tables is determined by this
                     parameter. 

     number_of_rssi_signals_under_tracked - 
         the time length in seconds used to determine whether the last reported
         timestamp of objects are valid and should be treated as existing in 
         the covered area

  Return value:

     None
 */

void hashtable_upload_location_to_database(
    HashTable * h_table,
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path,
    LocationInfoType location_type,
    int number_of_rssi_signals_under_tracked);

#endif
