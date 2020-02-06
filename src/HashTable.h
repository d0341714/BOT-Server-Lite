#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <pthread.h>
#include <string.h>

#include "HNode.h"
#include "Tabletype.h"
#include "BeDIS.h"
#include "SqlWrapper.h"

#define SLOTS_IN_MEM_POOL_HASH_TABLE_ROW 2048
#define SLOTS_IN_MEM_POOL_DataForHashtable_TABLE_ROW 2048
//totaltag*16
#define SLOTS_IN_MEM_POOL_UUID_RECORD_TABLE_ROW 65536
#define DRIFT_DISTANCE 50

Memory_Pool hash_table_row_mempool;
Memory_Pool uuid_record_table_row_mempool;
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
HashTable * panic_table;
/*

*/
typedef struct {	
   char* area_id;
   HashTable * area_hash_ptr;
} AreaTable;
/*
PanicMAC panic_mac_array[64];
int panic_mac_array_size=64;
int max_panic_address=0;
*/

AreaTable area_table[16];
int area_table_max_size;
int area_table_size;

// Designated initializer. Allows for control over max_load percentage, resize_factor, and init_size.
HashTable * hashtable_new(
	int init_size,
	double max_load,
	double resize_factor,
	EqualityFunc equal,
	HashFunc hash,
	DeleteData deleteKey,
	DeleteData deleteValue
);


// Convenience initializer. Uses default values for initial size, max load percentage, and resize factor. 
HashTable * hashtable_new_default(
	EqualityFunc equal,
	DeleteData deleteKey,
	DeleteData deleteValue
);

// frees memory for all keys, values and table its self. 
void hashtable_delete(HashTable * h_table);

// Iterates through all keys and values in the hashtable
void hashtable_iterate(HashTable * h_table, IteratorCallback callback);

//
void hashtable_go_through_panic(HashTable * h_table);





// Adds entry to hashtable. If entry already exists for given key, value is replaced with new value. 
void hashtable_put(HashTable * h_table, void * key, size_t key_len, void * value, size_t value_len);

// Gets value for key. If no entry exists, returns NULL
void * hashtable_get(HashTable * h_table, void * key, size_t key_len);

// Removes entry with given key. If no entry exists, does nothing.
void hashtable_remove(HashTable * h_table, void * key, size_t key_len);

// Checks whether an entry with given key exists in hashtable.
int hashtable_key_exists(HashTable * h_table, void * key, size_t key_len);

// Prints hashtable in user readable format. Interprets key as string and prints value as pointer. If you are not using string keys, you should create your own print function using the hashtable_iterate() function. 
void hashtable_print(HashTable * h_table);

int equal_string(void * a, void * b);

void destroy_nop(void * a);

// Function for server
void initial_area_table(void);

HashTable * hash_table_of_specific_area_id(char* area_id);

ErrorCode hashtable_update_object_tracking_data(char* buf,size_t buf_len);

int rssi_weight(int * rssi_array,int k,int head);

void hashtable_go_through_for_summarize(HashTable * h_table);

void hashtable_go_through_for_get_summary(HashTable * h_table,DBConnectionListHead *db_connection_list_head,char *server_installation_path);

void hashtable_put_mac_table(
	HashTable * h_table, 
	void * key, size_t key_len, 
	DataForHashtable * value, size_t value_len);
	

void hashtable_summarize_object_location(void);

void upload_hashtable_for_all_area(DBConnectionListHead *db_connection_list_head,char *server_installation_path);

#endif
