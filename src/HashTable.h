#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <pthread.h>
#include <string.h>

#include "HNode.h"
#include "Tabletype.h"
#include "BeDIS.h"
#include "SqlWrapper.h"
#include <math.h>

#define SLOTS_IN_MEM_POOL_HASH_TABLE_ROW 2048

#define SLOTS_IN_MEM_POOL_UUID_RECORD_TABLE_ROW 2048

#define INITIAL_AVERAGE_RSSI -100

#define LENGTH_OF_COORDINATE 9

#define INITIAL_AREA_TABLE_MAX_SIZE 256

#define AREA_ID_LENGTH 5

Memory_Pool hash_table_row_mempool;
Memory_Pool mac_address_mempool;
Memory_Pool DataForHashtable_mempool;

typedef uint32_t (* HashFunc)(const void *, size_t);
typedef int (* EqualityFunc)(void *, void *);
typedef void (* IteratorCallback)(void *, size_t, void *, size_t);


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


HashTable * area_hash_table;

typedef struct AreaTable{	
   char area_id[AREA_ID_LENGTH];
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

HashTable * hash_table_of_specific_area_id(char* area_id);

ErrorCode hashtable_update_object_tracking_data(char* buf, size_t buf_len);

int rssi_weight(int * rssi_array,int k,int head, int rssi_weight_multiplier);

void hashtable_go_through_for_summarize(
    HashTable * h_table,
    int rssi_weight_multiplier,
    int drift_rssi);

void hashtable_go_through_for_get_summary(
    HashTable * h_table,
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path,
    int ready_for_location_history_table);

void hashtable_put_mac_table(HashTable * h_table, 
                             void * key, 
                             size_t key_len, 
                             DataForHashtable * value, 
                             size_t value_len);

void upload_hashtable_for_all_area(
    DBConnectionListHead *db_connection_list_head, 
    char *server_installation_path,
    int upload_hashtable_for_all_area,
    int rssi_weight_multipier,
    int drift_rssi,
    int drift_distance);

void hashtable_go_through_for_get_location_history(
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path);

#endif
