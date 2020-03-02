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

/* The number of slots in the memory pool for the node of all the 
hashtables. Value of this constant must be bigger than the total number 
of objects to be tracked in the system. */
#define SLOTS_IN_MEM_POOL_HASH_TABLE_NODE 65536

/* The number of slots in the memory pool for the key part of all the 
hashtables. Value of this constant must be bigger than the total number 
of objects to be tracked in the system. */
#define SLOTS_IN_MEM_POOL_MAC_ADDRESS 65536

/* The number of slots in the memory pool for the value part of all the 
hashtables. Value of this constant must be bigger than the total number 
of objects to be tracked in the system. */
#define SLOTS_IN_MEM_POOL_HASH_TABLE_VALUE 65536

/* The number of entries in each hashtable. */
#define NUMBER_ENTRIES_IN_ONE_HASH_TABLE 256

/* The default average rssi vlaue for the newly created node in hashtable */
#define INITIAL_AVERAGE_RSSI -100

/* The default number of hashtables to be created to support covered areas 
in the system. */
#define INITIAL_AREA_TABLE_MAX_SIZE 4

/* Type of location information. */
typedef enum _LocationInfoType {

    LATEST_LOCATION_INFO = 0,
    LOCATION_FOR_HISTORY = 1,

} LocationInfoType;

/* Generic helper function declarations used in member of HNode structure */

typedef uint32_t (* HashFunc)(const void *, size_t);

typedef int (* EqualityFunc)(void *, void *);

typedef void (* IteratorCallback)(void *, size_t, void *, size_t);

typedef void (* DeleteData)(void *);

/* Structure for each node in hashtable */
typedef struct HNode_{
    void * key;
    size_t key_len;
    void * value;
    size_t value_len;
    DeleteData deleteKey;
    DeleteData deleteValue;
    struct HNode_ * next;
} HNode;

/* Structure for hashtable */ 
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

    /* The lock to protect all information within this specific covered 
    area_id */
    pthread_mutex_t * ht_mutex;

} HashTable;

/* Structure to store hashtable for each covered area separately */
typedef struct AreaTable{   

   int area_id;
   HashTable * area_hash_ptr;

} AreaTable;

/* Global variables */

/* The memory pool for the each node structure of hashtable. */
Memory_Pool hash_table_node_mempool;

/* The memory pool for the key part of all the hashtables. */
Memory_Pool mac_address_mempool;

/* The memory pool for the value part of all the hashtables. */
Memory_Pool hash_table_value_mempool;

/* The pointer to the head of the array of hashtables for all covered areas */
AreaTable* area_table;

/* The lock used to protect the array of hashtables for all covered areas */
pthread_mutex_t area_table_lock;

/* The maximum number of hashtables to be created in the system */
int area_table_max_size;

/* The first index of the area_table array which has not be used  */
int next_index_area_table;

/* Helper functions */

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

int equal_string(void * a, void * b);

void destroy_nop(void * a);

// Function for server

/*
  initialize_area_table:

     This function initializes the memory pools and all hashtables for all 
     covered areas.

  Parameters:

      None

  Return value:


      ErrorCode - Indicate the result of execution, the expected return code
                  is WORK_SUCCESSFULLY.

 */
ErrorCode initialize_area_table();

/*
  hash_table_of_specific_area_id:

     This function searches the area_table to find out and return the 
     corresponding hashtable for input area_id. It also creates new
     hashtable for the input area_id, if the hashtable does not exist.

  Parameters:

      area_id - the area_id embedded in the lbeacon uuid information

  Return value:


      ErrorCode - Indicate the result of execution, the expected return code
                  is WORK_SUCCESSFULLY.

 */
HashTable * hash_table_of_specific_area_id(int area_id);

/*
  hashtable_update_object_tracking_data:

     This function parses the input tracking data and uses the extracted
     information from the tracking data to update nodes in hashtable. If
     this functions detects panic status of objects, it immediately updates
     database for this emergency situation.

  Parameters:

     db_connection_list_head - the list head of database connection pool

     buf - the pointer to the input tracking data

     buf_len - the length of input buf

     value - the extracted scanned information of the pair of lbeacon uuid and 
             mac_address

     number_of_lbeacons_under_tracked - 
         the number of lbeacons to be kept in the arrary of recently scanned 
         lbeacon uuids to calculate location of objects 

     number_of_rssi_signals_under_tracked - 
         the time length in seconds used to determine whether the last reported
         timestamp of objects are valid and should be treated as existing in 
         the covered area

  Return value:

      ErrorCode - Indicate the result of execution, the expected return code
                  is WORK_SUCCESSFULLY.
 */

ErrorCode hashtable_update_object_tracking_data(
    DBConnectionListHead *db_connection_list_head,
    char* buf, 
    size_t buf_len, 
    const int number_of_lbeacons_under_tracked,
    const int number_of_rssi_signals_under_tracked);

/*
  hashtable_maintain_key_part:

     This function searches and maintains the input hashtable to find the input
     mac_address. It returns the index of the input mac_address in the input 
     hashtable.

  Parameters:

     h_table - the pointer to specific hashtable of one covered area

     key - the pointer to the input mac_address extracted from tracking data

     key_len - the length of input key

     number_of_lbeacons_under_tracked - 
         the number of lbeacons to be kept in the arrary of recently scanned 
         lbeacon uuids to calculate location of objects 

     number_of_rssi_signals_under_tracked - 
         the time length in seconds used to determine whether the last reported
         timestamp of objects are valid and should be treated as existing in 
         the covered area

  Return value:

     index of the input mac_address key in the input hashtable 
 */

int hashtable_maintain_key_part(HashTable * h_table, 
                                void * key, 
                                size_t key_len, 
                                const int number_of_lbeacons_under_tracked,
                                const int number_of_rssi_signals_under_tracked);

/*
  hashtable_put_mac_table:

     This function adds the tracking data with pair of lbeacon uuid and 
     mac_address information to nodes of hashtable. If the node of mac_address 
     exists, it updates the input lbeacon uuid to recently scanned lbeacon
     uuid array. Otherwise, it creates a new node in hashtable for the input
     mac_address first and then inserts the input lbeacon uuid as the first 
     lbeacon uuid in the array of recently scanned lbeacon uuids.

  Parameters:

     h_table - the pointer to specific hashtable of one covered area

     key - the pointer to the input mac_address extracted from tracking data

     key_len - the length of input key

     value - the extracted scanned information of the pair of lbeacon uuid and 
             mac_address

     number_of_lbeacons_under_tracked - 
         the number of lbeacons to be kept in the arrary of recently scanned 
         lbeacon uuids to calculate location of objects 

     number_of_rssi_signals_under_tracked - 
         the time length in seconds used to determine whether the last reported
         timestamp of objects are valid and should be treated as existing in 
         the covered area

  Return value:

     None
 */

void hashtable_put_mac_table(HashTable * h_table, 
                             const void * key, 
                             const size_t key_len, 
                             DataForHashtable * value, 
                             const int number_of_lbeacons_under_tracked,
                             const int number_of_rssi_signals_under_tracked);

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
                    const int rssi_weight_multiplier);

/*
  get_average_rssi:

     This function calculates the running average of rssi signals in the array
     of rssi_array without considering the dramatical rssi values.

  Parameters:

     rssi_array - the array of rssi signals 

     number_of_rssi_signals_under_tracked - 
         the number of rssi signals which are kept in hashtable to calculate
         location of objects. This setting is configurable in server.conf

     unreasonable_rssi_change - the abnormal rssi signal strength change in 
                                adjacent seconds. When this happens, the rssi
                                singal will be ingored in the calculation.

  Return value:

     The running average of rssi signals in the input rssi_array array
 */
int get_average_rssi(const int *rssi_array,
                     const int number_of_rssi_signals_under_tracked,
                     const int unreasonable_rssi_change);

/*
  hashtable_summarize_location_information:

     This function determines the lbeacon uuid closest to objects and 
     calculates the estimated coordinate_x and coordinate_y of 
     objects.

  Parameters:

     h_table - the pointer to specific hashtable of one covered area

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

void hashtable_summarize_location_information(
    HashTable * h_table,
    const int number_of_rssi_signals_under_tracked,
    const int unreasonable_rssi_change,
    const int rssi_weight_multiplier,
    const int rssi_difference_of_location_accuracy_tolerance,
    const int drift_distance);

/*
  hashtable_traverse_areas_to_upload_latest_location:

     This function traverses hashtable of all covered areas to
     upload current location information of objects to current summary 
     location table in the database.

  Parameters:

     db_connection_list_head - the list head of database connection pool

     server_installation_path - the installation of server

     area_set - the list of areas under with the location information of 
                objects should be summarized and uploaded

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

void hashtable_traverse_areas_to_upload_latest_location(
    DBConnectionListHead *db_connection_list_head,
    const char *server_installation_path,
    AreaSet *area_set,
    const int number_of_rssi_signals_under_tracked,
    const int unreasonable_rssi_change,
    const int rssi_weight_multiplier,
    const int rssi_difference_of_location_accuracy_tolerance,
    const int drift_distance);

/*
  hashtable_traverse_areas_to_upload_history_data:

     This function traverses hashtable of all covered areas to
     upload current location information of objects to history table
     in the database.

  Parameters:

     db_connection_list_head - the list head of database connection pool

     server_installation_path - the installation of server

     area_set - the list of areas under with the location information of 
                objects should be uploaded

     number_of_rssi_signals_under_tracked -
         the time length in seconds used to determine whether the last reported
         timestamp of objects are valid and should be treated as existing in 
         the covered area

  Return value:

     None

 */

void hashtable_traverse_areas_to_upload_history_data(
    DBConnectionListHead *db_connection_list_head,
    const char *server_installation_path,
    AreaSet *area_set,
    const int number_of_rssi_signals_under_tracked);

/*
  hashtable_upload_location_to_database:

     This function travers all nodes in the input specific hashtable to upload 
     the location information all objects to database. The LocationInfoType is
     used to specified the destination of database tables.

  Parameters:

     h_table - the pointer to specific hashtable of one covered area

     area_id - area id of specific covered area

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
    int area_id, 
    DBConnectionListHead *db_connection_list_head,
    const char *server_installation_path,
    const LocationInfoType location_type,
    const int number_of_rssi_signals_under_tracked);

#endif
