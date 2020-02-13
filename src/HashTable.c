#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#include "HashTable.h"

// private interface

static void _hashtable_resize(HashTable * h_table);
static uint32_t _hashtable_hash_adler32(const void *buf, size_t buflength);
static int _hashtable_replace(
	HashTable * h_table, 
	void * key, size_t key_len, 
	void * value, size_t value_len
);

static int _hashtable_replace_uuid(
	HashTable * h_table, 
	void * key, size_t key_len, 
	void * value, size_t value_len
);

// implementation

HashTable * hashtable_new(
	int init_size,
	double max_load,
	double resize_factor,
	EqualityFunc equal,
	HashFunc hash,
	DeleteData deleteKey,
	DeleteData deleteValue
) {
	HashTable * ht = calloc(sizeof(HashTable), 1);
	if (ht != 0) {
		ht->table = calloc(sizeof(HNode *), init_size);
		ht->size = init_size;
		ht->count = 0;
		ht->equal = equal;
		ht->hash = hash;
		ht->max_load = max_load;
		ht->resize_factor = resize_factor;
		ht->deleteKey = deleteKey;
		ht->deleteValue = deleteValue;
		ht->ht_mutex = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(ht->ht_mutex, 0);
	}
	return ht;
}

HashTable * hashtable_new_default(
	EqualityFunc equal,
	DeleteData deleteKey,
	DeleteData deleteValue
) {
	return hashtable_new(
		1000, 1, 1, 
		equal, _hashtable_hash_adler32, 
		deleteKey, deleteValue
	);
}
/**/
void hashtable_delete(HashTable * h_table) {
	int size;
	HNode ** table;
	int i;
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	pthread_mutex_lock(ht_mutex);
	
	size = h_table->size;
	table = h_table->table;
	
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		hnode_delete_list(curr);
	}
	free(table);
	free(h_table);

	pthread_mutex_unlock(ht_mutex);
	pthread_mutex_destroy(ht_mutex);
}

void hashtable_put(
	HashTable * h_table, 
	void * key, size_t key_len, 
	void * value, size_t value_len) 
{	/*
	int res;
	uint32_t hash_val;
	uint32_t index;
	HNode * prev_head;
	HNode * new_head;
	//try to replace existing key's value if possible
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	pthread_mutex_lock(ht_mutex);
	
	res = _hashtable_replace(h_table, key, key_len, value, value_len);
	if (res == 0) {
		//  resize if needed. 
		double load = ((double) h_table->count) / ((double) h_table->size);
		if (load > h_table->max_load) {
			_hashtable_resize(h_table);
		}

		//code to add key and value in O(1) time.
		hash_val = h_table->hash(key, key_len);
		index = hash_val % h_table->size;
		prev_head = h_table->table[index];
		
		new_head = hnode_new(
			key, key_len,
			value, value_len,
			h_table->deleteKey,
			h_table->deleteValue
		);

		new_head->next = prev_head;
		h_table->table[index] = new_head;
		h_table->count = h_table->count + 1;
	}
	
	pthread_mutex_unlock(ht_mutex);*/
	//printf("inserting, {%s => %p}\n", key, value);
	//print_HashTable(h_table);
}

void * hashtable_get(HashTable * h_table, void * key, size_t key_len) {
	/*uint32_t hash_val;
	uint32_t index;
	HNode * curr;
	int res;
	void * ret_val;
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	pthread_mutex_lock(ht_mutex);
	
	hash_val = h_table->hash(key, key_len);
	index = hash_val % h_table->size;
	curr = h_table->table[index];
	ret_val = 0;
	while (curr) {
		res = h_table->equal(curr->key, key);
		if (res == 1) {
			ret_val = curr->value;
		}
		curr = curr->next;
	}
	
	pthread_mutex_unlock(ht_mutex);
	return ret_val;*/
}

static int _hashtable_replace(
	HashTable * h_table, 
	void * key, size_t key_len, 
	void * value, size_t value_len
) {
	uint32_t hash_val = h_table->hash(key, key_len);
	uint32_t index = hash_val % h_table->size;
	HNode * curr = h_table->table[index];
	while (curr) {
		int res = h_table->equal(curr->key, key);
		if (res == 1) {
			curr->value = value;
			curr->value_len = value_len;
			return 1;
		}
		curr = curr->next;
	}
	return 0;
}
/*

*/
static int _hashtable_replace_uuid(
	HashTable * h_table, 
	void * key, size_t key_len, 
	DataForHashtable * value, size_t value_len
) {	
	int i=0;
	int j;
	int head;
	int *rssi_array;
	int time_gap=0;
	//char* lbeacon_uuid;
	int record_table_size;
	int invalid_place=-1;
	hash_table_row* exist_MAC_address_row;
	uint32_t hash_val = h_table->hash(key, key_len);
	uint32_t index = hash_val % h_table->size;
	HNode * curr = h_table->table[index];
	char coordinateX[LENGTH_OF_COORDINATE];
	char coordinateY[LENGTH_OF_COORDINATE];
		
	while (curr) {		
		int res = h_table->equal(curr->key, key);
		if (res == 1) {			
			exist_MAC_address_row=curr->value;
			record_table_size = exist_MAC_address_row->record_table_size;
			strcpy(exist_MAC_address_row->battery,value->battery_voltage);
			strcpy(exist_MAC_address_row->panic_button,value->panic_button);
			strcpy(exist_MAC_address_row->final_timestamp,value->final_timestamp_GMT);
			
			//匹配uuid
			for(i=0;i<record_table_size;i++){
				if(exist_MAC_address_row->uuid_record_table_array[i].valid==1){
					if(strcmp(value->lbeacon_uuid,exist_MAC_address_row->uuid_record_table_array[i].uuid)==0){//跟傳進來的uuid比較依樣
					
					time_gap=atoi(value->final_timestamp_GMT)-atoi(exist_MAC_address_row->uuid_record_table_array[i].final_timestamp);
					if(time_gap>0){
						strcpy(exist_MAC_address_row->uuid_record_table_array[i].final_timestamp,value->final_timestamp_GMT);
						
						head=exist_MAC_address_row->uuid_record_table_array[i].head;
						for(j=0;j<time_gap;j++){
							head++;
							if(head==SIZE_OF_RSSI_ARRAY){
								head=0;
							}								
							exist_MAC_address_row->uuid_record_table_array[i].rssi_array[head]=0;	
						}
						exist_MAC_address_row->uuid_record_table_array[i].rssi_array[head]=atoi(value->rssi);
						exist_MAC_address_row->uuid_record_table_array[i].head=head;
					}			
					
										
					return 1;
					
					}
				}else{
					invalid_place=i;
				}
					
				
			}
			
			//不再裡面,寫入invalid或擴增空間
			if(invalid_place!=-1){
				
				strcpy(exist_MAC_address_row->uuid_record_table_array[invalid_place].uuid,value->lbeacon_uuid);
				strcpy(exist_MAC_address_row->uuid_record_table_array[invalid_place].initial_timestamp,value->initial_timestamp_GMT);
				strcpy(exist_MAC_address_row->uuid_record_table_array[invalid_place].final_timestamp,value->final_timestamp_GMT);
				exist_MAC_address_row->uuid_record_table_array[invalid_place].rssi_array[0]=atoi(value->rssi);
				exist_MAC_address_row->uuid_record_table_array[invalid_place].head=0;
				memcpy(coordinateX,value->lbeacon_uuid+12,8);
				coordinateX[8]='\0';
				exist_MAC_address_row->uuid_record_table_array[invalid_place].coordinateX=atof(coordinateX);				
				memcpy(coordinateY,value->lbeacon_uuid+24,8);
				coordinateY[8]='\0';
				exist_MAC_address_row->uuid_record_table_array[invalid_place].coordinateY=atof(coordinateY);
				
				exist_MAC_address_row->uuid_record_table_array[invalid_place].valid=1;
				curr->value=&exist_MAC_address_row;
				curr->value_len = sizeof(exist_MAC_address_row);
				
			}else{
				exist_MAC_address_row->record_table_size*=2;
				//resize 還要改
				//exist_MAC_address_row->uuid_record_table_array=( uuid_record_table_row*)realloc(exist_MAC_address_row->uuid_record_table_array,(exist_MAC_address_row->record_table_size*sizeof(uuid_record_table_row)));
				zlog_error(category_debug,"resize uuid table %d",exist_MAC_address_row->record_table_size);
				strcpy(exist_MAC_address_row->uuid_record_table_array[i].uuid,value->lbeacon_uuid);
				strcpy(exist_MAC_address_row->uuid_record_table_array[i].initial_timestamp,value->initial_timestamp_GMT);
				strcpy(exist_MAC_address_row->uuid_record_table_array[i].final_timestamp,value->final_timestamp_GMT);
				exist_MAC_address_row->uuid_record_table_array[i].rssi_array[0]=atoi(value->rssi);
				exist_MAC_address_row->uuid_record_table_array[i].head=0;
				strncpy(coordinateX,value->lbeacon_uuid+12,8);
				exist_MAC_address_row->uuid_record_table_array[invalid_place].coordinateX=atof(coordinateX);
				strncpy(coordinateY,value->lbeacon_uuid+24,8);
				exist_MAC_address_row->uuid_record_table_array[invalid_place].coordinateY=atof(coordinateY);
				//curr->value_len = sizeof(*exist_MAC_address_row);
				curr->value=&exist_MAC_address_row;
				curr->value_len = sizeof(exist_MAC_address_row);
			}
			/*zlog_error(category_debug,"mac %s uuid %s final %s ",
							(char*)key,lbeacon_uuid,exist_MAC_address_row->uuid_record_table_array[i]->final_timestamp);
			*/					
			
			return 1;
		}
		curr = curr->next;
	}
	//不存在
	return 0;
}
//x
void hashtable_remove(HashTable * h_table, void * key, size_t key_len) {
	uint32_t hash_val ;
	uint32_t index ;
	HNode ** table ;
	HNode * curr ;
	HNode * prev ;
	
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	pthread_mutex_lock(ht_mutex);

	hash_val = h_table->hash(key, key_len);
	index = hash_val % h_table->size;
	table = h_table->table;
	curr = table[index];
	prev = 0;

	//elem to delete is first in list
	if (h_table->equal(curr->key, key)) {
		table[index] = curr->next;
		hnode_delete_single(curr);
		h_table->count = h_table->count - 1;
		pthread_mutex_unlock(ht_mutex);
		return;
	}

	//if elem to delete is NOT first
	while (curr != 0) {
		if (h_table->equal(curr->key, key)) {
			prev->next = curr->next;
			hnode_delete_single(curr);
			h_table->count = h_table->count - 1;
            pthread_mutex_unlock(ht_mutex);
			return;
		}
		//below code must trigger once
		prev = curr;
		curr = curr->next;
	}

	pthread_mutex_unlock(ht_mutex);
}

static void _hashtable_resize(HashTable * h_table) {
	int i = 0;
	int size = h_table->size;
	int new_size = (int)(h_table->size * h_table->resize_factor);
	HNode ** new_table = calloc(sizeof(HNode *), new_size);
	HNode ** old_table = h_table->table;

	h_table->table = new_table;
	h_table->size = new_size;
	h_table->count = 0;

	
	for (i = 0; i < size; i++) {
		HNode * curr = old_table[i];
		while (curr != 0) {
			HNode * prev = curr;
			curr = curr->next;

			hashtable_put(h_table, prev->key, prev->key_len, prev->value, prev->value_len);
			//free the memory for only the struct
			//note, this does NOT free the key or the value
			free(prev);
		}
	}
	//free the memory for the old array
	free(old_table);
}

void hashtable_iterate(HashTable * h_table, IteratorCallback callback) {
	int size;
	HNode ** table;
	int i;
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	pthread_mutex_lock(ht_mutex);

	size = h_table->size;
	table = h_table->table;
	i = 0;
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {
			callback(curr->key, curr->key_len, curr->value, curr->value_len);
			curr = curr->next;
		}
	}

	pthread_mutex_unlock(ht_mutex);
}
//go through
void hashtable_print(HashTable * h_table) {
	int size = h_table->size;
	HNode ** table = h_table->table;
	int i = 0;
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {
			// callback(curr->key, curr->key_len, curr->value, curr->value_len);
			printf("'%s' => %p,\n", (char *)curr->key, curr->value);
			curr = curr->next;
		}
	}
}


/*
adler32 code taken from Stack Overflow post

http://stackoverflow.com/a/14409947/3158248

Please note, that the rest of the code is my own.
*/

static uint32_t _hashtable_hash_adler32(const void *buf, size_t buflength) {
	 char * buffer;
	 uint32_t s1;
	 uint32_t s2;
	 size_t n;
     buffer = (const char*)buf;

     s1 = 1;
     s2 = 0;    
     
     
     for (n = 0; n < buflength; n++) {
        s1 = (s1 + buffer[n]) % 65521;
        s2 = (s2 + s1) % 65521;
     }     
     return (s2 << 16) | s1;
}


int equal_string(void * a, void * b) {
	char * str1 = (char *) a;
	char * str2 = (char *) b;
	int res = strcmp(str1, str2);
	if (res == 0) {return 1;}
	else {return 0;}
}

// useful for non-heap allocated data
void destroy_nop(void * a) {
	return;
}
/*
initial area table
*/
void initial_area_table(void){
	
	int i;
	zlog_error(category_debug,">>initial_area_table");	
	if(MEMORY_POOL_SUCCESS != mp_init( &hash_table_row_mempool, 
                                       sizeof(hash_table_row), 
                                       SLOTS_IN_MEM_POOL_HASH_TABLE_ROW))
    {
        zlog_error(category_debug,"initial fail hash_table_row_mempool");
		return E_MALLOC;
    }
	
	if(MEMORY_POOL_SUCCESS != mp_init( &mac_address_mempool, 
                                       LENGTH_OF_MAC_ADDRESS, 
                                      SLOTS_IN_MEM_POOL_HASH_TABLE_ROW))
    {
        zlog_error(category_debug,"initial fail mac_address_mempool");
		return E_MALLOC;
    }
	
	if(MEMORY_POOL_SUCCESS != mp_init( &DataForHashtable_mempool, 
                                       sizeof(DataForHashtable), 
                                       SLOTS_IN_MEM_POOL_UUID_RECORD_TABLE_ROW))
    {
        zlog_error(category_debug,"initial fail DataForHashtable_mempool");
		return E_MALLOC;
    }
	
	area_table_max_size=INITIAL_AREA_TABLE_MAX_SIZE;
	area_table_size=0;
	/*for(i=0;i<area_table_max_size;i++){
		memset(area_table[i].area_id,'\0',AREA_ID_LENGTH);
	}*/
	zlog_error(category_debug,"initial_area_table successful");	
	return;
}

HashTable * hash_table_of_specific_area_id(char* area_id){
	int i;
	int exist=0;
	HashTable * h_table;
	
	zlog_error(category_debug,"area id %s",area_id);
	
	for(i=0;i<area_table_max_size;i++){
		if(area_table[i].area_id==NULL) continue;
		exist = strcmp(area_table[i].area_id,area_id);
		if(exist==0){
			return area_table[i].area_hash_ptr;
		}
	}
	//不存在
	h_table = hashtable_new_default(
		equal_string, 
		destroy_nop, 
		destroy_nop
		);
	
	if(area_table_max_size>area_table_size){
		
		area_table[area_table_size].area_hash_ptr=h_table;
		strcpy(area_table[area_table_size].area_id,area_id);
		
		area_table_size++;
	}else{
		//resize
		area_table_max_size*=2;
		//resize還要改
		//area_table=realloc(area_table,area_table_max_size*sizeof(AreaTable));
		area_table[area_table_size].area_hash_ptr=h_table;
		strcpy(area_table[area_table_size].area_id,area_id);
		
		area_table_size++;		
		 
	}
	return h_table;
}

/*
取代SQL_update_object_tracking_data
*/
ErrorCode hashtable_update_object_tracking_data(char* buf,size_t buf_len){
	ErrorCode ret_val = WORK_SUCCESSFULLY;
    
    int num_types = 2; // BR_EDR and BLE types
    /**/
	char temp_buf[WIFI_MESSAGE_LENGTH];
	char *saveptr = NULL;
	//int num_types = 2;
	
    char *lbeacon_uuid = NULL;
    char *lbeacon_ip = NULL;
    char *lbeacon_timestamp = NULL;
    char *object_type = NULL;
    char *object_number = NULL;
    int numbers = 0;
    char *object_mac_address = NULL;
    char *initial_timestamp_GMT = NULL;
    char *final_timestamp_GMT = NULL;
    char *rssi = NULL;
    char *panic_button = NULL;
	char* battery_voltage;
    int current_time = get_system_time();
    int lbeacon_timestamp_value;	
	char area_id[5];
	HashTable * area_table_ptr;
	
	DataForHashtable* data_row;
	int i;
	
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);
	
	zlog_error(category_debug,">>hashtable_update_object_tracking_data");
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
	//printf("lbeacon_timestamp: %s\n",lbeacon_timestamp);
    if(lbeacon_timestamp == NULL){
        return E_API_PROTOCOL_FORMAT;
    }
    lbeacon_timestamp_value = atoi(lbeacon_timestamp);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
	
    while(num_types --){
		
        object_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        object_number = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        zlog_debug(category_debug, "object_type=[%s], object_number=[%s]", 
                   object_type, object_number);
        if(object_number == NULL){            
            return E_API_PROTOCOL_FORMAT;
        }
        numbers = atoi(object_number);
        while(numbers--){
			
            object_mac_address = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            initial_timestamp_GMT = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            final_timestamp_GMT = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            rssi = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
			panic_button = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
            battery_voltage = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
			
			data_row=mp_alloc(&DataForHashtable_mempool);
			if(data_row==NULL) {
				printf("=mp_alloc(&DataForHashtable_mempool) fail\n");
				return 0;
			}
			memset(data_row,0,sizeof(DataForHashtable));
			//lock
			pthread_mutex_init( &data_row->list_lock, 0);
			
			
			memcpy(area_id, lbeacon_uuid, 4);
			//strcat(area_id,'\0');
			area_id[4]='\0';
			
			data_row->lbeacon_uuid=lbeacon_uuid;
			data_row->initial_timestamp_GMT=initial_timestamp_GMT;
			data_row->final_timestamp_GMT=final_timestamp_GMT;
			data_row->rssi=rssi;
			data_row->battery_voltage=battery_voltage;
			data_row->panic_button=panic_button;
			//拆解ok
			zlog_error(category_debug,"key %s 12345:%s %s %s %s %s\n",object_mac_address ,data_row->lbeacon_uuid,data_row->initial_timestamp_GMT,data_row->final_timestamp_GMT,data_row->rssi,data_row->battery_voltage);
			/**/
			//pthread_mutex_lock(&data_row->list_lock);
			printf("area:%s\n",area_id);
			area_table_ptr=hash_table_of_specific_area_id(area_id);
			
			hashtable_put_mac_table(area_table_ptr, 
							object_mac_address,sizeof(*object_mac_address), 
							data_row, sizeof(*data_row));	
			mp_free(&DataForHashtable_mempool,data_row);			
			
        }
    }

    
    return WORK_SUCCESSFULLY;
}
/*
改成計算weight?

*/
int rssi_weight(int * rssi_array,int k,int head){
	
	if(rssi_array[k]==0) return 0;
	
	if((k+SIZE_OF_RSSI_ARRAY-1)%SIZE_OF_RSSI_ARRAY!=head)		
		if(rssi_array[(k+SIZE_OF_RSSI_ARRAY-1)%SIZE_OF_RSSI_ARRAY]!=0)
			if(abs(rssi_array[k]-rssi_array[(k+SIZE_OF_RSSI_ARRAY-1)%SIZE_OF_RSSI_ARRAY])>UNRESAONABLE_RSSI)
				return 0;
	
	
	
	if(rssi_array[k]>-50)
		return 32;
	else if(rssi_array[k]>-60)
		return 16;
	else if(rssi_array[k]>-70)
		return 8;
	else if(rssi_array[k]>-80)
		return 4;
	else if(rssi_array[k]>-90)
		return 2;
	else if(rssi_array[k]>=-100)
		return 1;
	return 0;
}

//go through
void hashtable_go_through_for_summarize(HashTable * h_table) {
	int size = h_table->size;
	HNode ** table = h_table->table;
	int i = 0;
	int j=0;
	int k=0;
	int l=0;
	int sum;
	char* uuid;
    char* initial_timestamp;
    char* final_timestamp;
	int sum_rssi;
	int avg_rssi;
	float weight_x;
	float weight_y;
	int weight_count;
	int weight_count_for_specific_uuid;
	int valid_rssi_count;	
	int weight;
	float summary_coordinateX_this_turn;
	float summary_coordinateY_this_turn;
	int recently_scanned;
	hash_table_row* table_row;	
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;	
	pthread_mutex_lock(ht_mutex);		
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {//every mac		
			table_row = curr->value;			
			sum_rssi=0;
			avg_rssi=INITIAL_AVERAGE_RSSI;
			valid_rssi_count=0;				
			
			//original summary uuid
			for(l=0;l<table_row->record_table_size;l++){
				if(table_row->uuid_record_table_array[l].valid!=0 &&
				   strcmp(table_row->uuid_record_table_array[l].uuid,table_row->summary_uuid)==0){
						if(atoi(table_row->uuid_record_table_array[l].final_timestamp)<(get_clock_time()-10)){							
							break;
						}
						for(k=0;k < SIZE_OF_RSSI_ARRAY;k++){
							if(rssi_weight(table_row->uuid_record_table_array[l].rssi_array,k,table_row->uuid_record_table_array[l].head)>0){
								sum_rssi+=table_row->uuid_record_table_array[l].rssi_array[k];
								valid_rssi_count++;								
							}
						}
						avg_rssi=sum_rssi/valid_rssi_count;						
						table_row->average_rssi=avg_rssi;
						break;
					
				}
			}
			
			
			j=0;			
			weight_x=0;
			weight_y=0;
			weight_count=0;
			recently_scanned=0;
			
			while(j<table_row->record_table_size){//不同uuid
				sum_rssi=0;
				valid_rssi_count=0;				
				if(table_row->uuid_record_table_array[j].valid==0) {
					j++;
					continue;	
				}
				
				//delete old data
				if(atoi(table_row->uuid_record_table_array[j].final_timestamp)<(get_clock_time()-10)){					
					table_row->uuid_record_table_array[j].valid=0;
					j++;
					continue;
				}  		
				
				weight_count_for_specific_uuid=0;
				//average
				for(k=0;k<SIZE_OF_RSSI_ARRAY;k++){
					//check is reasonable
					weight=rssi_weight(table_row->uuid_record_table_array[j].rssi_array,k,table_row->uuid_record_table_array[j].head);
					if(weight>0){
						sum_rssi+=table_row->uuid_record_table_array[j].rssi_array[k];
						weight_count_for_specific_uuid+=weight;
						valid_rssi_count++;
						
						recently_scanned=1;
					}					
				}
				
				if(valid_rssi_count==0) {
					j++;
					continue;
				}
				
				weight_x+=table_row->uuid_record_table_array[j].coordinateX * weight_count_for_specific_uuid;
				weight_y+=table_row->uuid_record_table_array[j].coordinateY * weight_count_for_specific_uuid;				
				weight_count+=weight_count_for_specific_uuid;
				
				if((sum_rssi/valid_rssi_count)-avg_rssi > DRIFT_RSSI){
					strcpy(table_row->summary_uuid, table_row->uuid_record_table_array[j].uuid);
					strcpy(table_row->initial_timestamp,table_row->uuid_record_table_array[j].initial_timestamp);
					//table_row->final_timestamp=uuid_table->final_timestamp;
					table_row->average_rssi=sum_rssi/valid_rssi_count;
				}
				
				j++;
			}
			
			table_row->recently_scanned=recently_scanned;
			if(weight_count!=0){
				summary_coordinateX_this_turn=weight_x/(float)weight_count;
				summary_coordinateY_this_turn=weight_y/(float)weight_count;
				
								
				if(abs(summary_coordinateX_this_turn - table_row->summary_coordinateX)>DRIFT_DISTANCE ||
					abs(summary_coordinateY_this_turn - table_row->summary_coordinateY)>DRIFT_DISTANCE){
					//coordinateX
					table_row->summary_coordinateX=summary_coordinateX_this_turn;
					//coordinateY
					table_row->summary_coordinateY=summary_coordinateY_this_turn;
				}				
				
				
			}			
			
			curr = curr->next;
		}
	}		
		pthread_mutex_unlock(ht_mutex);
}

void hashtable_go_through_for_get_summary(
		HashTable * h_table,DBConnectionListHead *db_connection_list_head,char *server_installation_path,
		int ready_for_location_history_table) {
	int size = h_table->size;
	HNode ** table = h_table->table;
	
	int i = 0;
	
	char filename[MAX_PATH];
	FILE *file = NULL;
	char location_filename[MAX_PATH];
	FILE *location_file = NULL;
	
	time_t rawtime;
    struct tm ts;
    char buf_initial_time[80];
    char buf_final_time[80];
	char buf_record_time[80];
					 
	hash_table_row* table_row;		
	
	sprintf(filename, "%s/temp/track_%d", 
									server_installation_path, 
									pthread_self());			
	file = fopen(filename, "wt");
	if(file == NULL){
		zlog_error(category_debug, "cannot open filepath %s", filename);
		return ;
	}
	
	if(ready_for_location_history_table==1){
		sprintf(location_filename, "%s/temp/locationtrack_%d", 
									server_installation_path, 
									pthread_self());			
		location_file = fopen(location_filename, "wt");
		if(file == NULL){
			zlog_error(category_debug, "cannot open filepath %s", filename);
			return ;
		}
	}
	
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {			
			table_row = curr->value;			
			if(table_row->recently_scanned>0){	
				zlog_error(category_debug,"mac %s table_row size:%d",curr->key,sizeof(curr->key));
				zlog_error(category_debug,"summary:%s %s %s %s %d %s\n"
							,table_row->summary_uuid,table_row->battery,
							table_row->initial_timestamp,table_row->final_timestamp,
							table_row->average_rssi,table_row->panic_button);
				printf("'%s' ", curr->key);
				printf("summary:%s %s %s %s %d %s\n",
							table_row->summary_uuid,table_row->battery,
							table_row->initial_timestamp,table_row->final_timestamp,
							table_row->average_rssi,table_row->panic_button);
	
				rawtime = atoi(table_row->initial_timestamp);
				ts = *gmtime(&rawtime);
				strftime(buf_initial_time, sizeof(buf_initial_time), 
						 "%Y-%m-%d %H:%M:%S", &ts);
            
				rawtime = atoi(table_row->final_timestamp);
				ts = *gmtime(&rawtime);
				strftime(buf_final_time, sizeof(buf_final_time), 
						 "%Y-%m-%d %H:%M:%S", &ts);
				
				fprintf(file, "%s,%d,%s,%s,%s,%s,%d,%d %s\n",
						table_row->summary_uuid,
						table_row->average_rssi,
						table_row->panic_button,
						table_row->battery,
						buf_initial_time,
						buf_final_time,
						(int)table_row->summary_coordinateX,
						(int)table_row->summary_coordinateY,
						(char *)curr->key);
				
				//location history file
				if(ready_for_location_history_table==1){
                    
					rawtime = atoi(get_clock_time());
					ts = *gmtime(&rawtime);
					strftime(buf_record_time, sizeof(buf_record_time), 
							"%Y-%m-%d %H:%M:%S", &ts);
							
					fprintf(location_file, "%s,%s,%s,%s,%d,%d\n",
							curr->key,
							table_row->summary_uuid,
							buf_record_time,
							table_row->battery,
							(int)table_row->summary_coordinateX,
							(int)table_row->summary_coordinateY);
				}
				
			}				
			
			curr = curr->next;
		}
	
	}	
	fclose(file);	
	/*SQL_upload_hashtable_summarize(db_connection_list_head,
									filename);*/
									
	if(ready_for_location_history_table==1){
		fclose(location_file);		
		/*SQL_upload_hashtable_summarize(db_connection_list_head,
										location_filename);*/
	}	
}
/*
只更改時間,rssi....

*/
void hashtable_put_mac_table(
	HashTable * h_table, 
	void * key, size_t key_len, 
	DataForHashtable * value, size_t value_len)	
{	
	int res;
	uint32_t hash_val;
	uint32_t index;
	HNode * prev_head;
	HNode * new_head;
	hash_table_row* hash_table_row_for_new_MAC;
	char coordinateX[LENGTH_OF_COORDINATE];
	char coordinateY[LENGTH_OF_COORDINATE];
	char* MAC_address;
	int i;
	//try to replace existing key's value if possible
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;	
	pthread_mutex_lock(ht_mutex);
	zlog_error(category_debug,">>hashtable_put_mac_table");
	res = _hashtable_replace_uuid(h_table, key, key_len, value, sizeof(*value));
	//for a new uuid
	if (res == 0 ) {
		
		//  resize if needed. 
		/*
		double load = ((double) h_table->count) / ((double) h_table->size);
		//if (load > h_table->max_load) {
		if((double)h_table->count > (double)h_table->size){
			pthread_mutex_unlock(ht_mutex);
			printf("resize!!!!!!!!!!!!!! %f %f\n",load,h_table->max_load);
			return;
			
			_hashtable_resize(h_table);
		}
		*/		
		//code to add key and value in O(1) time.
		hash_val = h_table->hash(key, key_len);
		index = hash_val % h_table->size;
		prev_head = h_table->table[index];	
		
		new_head = malloc(sizeof(HNode));
		memset(new_head,0,sizeof(HNode));
		if (new_head != 0 ) {			
			MAC_address=mp_alloc(&mac_address_mempool);
			memset(hash_table_row_for_new_MAC,0,sizeof(hash_table_row));
			strcpy(MAC_address,key);
			
			hash_table_row_for_new_MAC=mp_alloc(&hash_table_row_mempool);
			memset(hash_table_row_for_new_MAC,0,sizeof(hash_table_row));
			
			strcpy(hash_table_row_for_new_MAC->summary_uuid , value->lbeacon_uuid);
			strcpy(hash_table_row_for_new_MAC->panic_button,value->panic_button);	
			/* Construct UUID as aaaa00zz0000xxxxxxxx0000yyyyyyyy format to represent 
									 1			13-20		25-32
			   lbeacon location. In the UUID format, aaaa stands for the area id, zz 
			   stands for the floor level with lowest_basement_level adjustment, 
			   xxxxxxxx stands for the relative longitude, and yyyyyyyy stands for
			   the relative latitude.
			*/
			memcpy(coordinateX,(value->lbeacon_uuid)+12,8);
			coordinateX[LENGTH_OF_COORDINATE-1]='\0';
			
			hash_table_row_for_new_MAC->summary_coordinateX =atof(coordinateX);
			memcpy(coordinateY,(value->lbeacon_uuid)+24,8);
			coordinateY[LENGTH_OF_COORDINATE-1]='\0';	
			hash_table_row_for_new_MAC->summary_coordinateY=atof(coordinateY);
			strcpy(hash_table_row_for_new_MAC->battery,value->battery_voltage);
			hash_table_row_for_new_MAC->average_rssi=INITIAL_AVERAGE_RSSI;
			strcpy(hash_table_row_for_new_MAC->initial_timestamp,value->initial_timestamp_GMT);			
			strcpy(hash_table_row_for_new_MAC->final_timestamp,value->final_timestamp_GMT);
			
			
			
			hash_table_row_for_new_MAC->recently_scanned=0;
			strcpy(hash_table_row_for_new_MAC->uuid_record_table_array[0].uuid,value->lbeacon_uuid);				
			strcpy(hash_table_row_for_new_MAC->uuid_record_table_array[0].initial_timestamp,value->initial_timestamp_GMT);
			strcpy(hash_table_row_for_new_MAC->uuid_record_table_array[0].final_timestamp,value->final_timestamp_GMT);
			hash_table_row_for_new_MAC->uuid_record_table_array[0].head=0;
			hash_table_row_for_new_MAC->uuid_record_table_array[0].valid=1;			
			hash_table_row_for_new_MAC->uuid_record_table_array[0].rssi_array[0]=atoi(value->rssi);			
			hash_table_row_for_new_MAC->uuid_record_table_array[0].coordinateX =atof(coordinateX);			
			hash_table_row_for_new_MAC->uuid_record_table_array[0].coordinateY=atof(coordinateY);
			hash_table_row_for_new_MAC->record_table_size=INITIAL_RECORD_TABLE_SIZE;
			for(i=1;i<INITIAL_RECORD_TABLE_SIZE;i++){
				hash_table_row_for_new_MAC->uuid_record_table_array[i].valid=0;
			}
			new_head->key = MAC_address;
			new_head->key_len =LENGTH_OF_MAC_ADDRESS;
			
			new_head->value = hash_table_row_for_new_MAC;
			new_head->value_len=sizeof(*hash_table_row_for_new_MAC);
			
			
			new_head->deleteKey = h_table->deleteKey;
			new_head->deleteValue = h_table->deleteValue;
			new_head->next=0;
			/**/

		}
		zlog_error(category_debug,"MAC %s ,size %d",new_head->key,sizeof(*hash_table_row_for_new_MAC));
		new_head->next = prev_head;
		h_table->table[index] = new_head;
		h_table->count = h_table->count + 1;
		
	}
	
	zlog_error(category_debug,"hashtable_put_mac_table<<");
	pthread_mutex_unlock(ht_mutex);
	
}
/*
	每秒
	跑過全部的hashtable
*/	
void hashtable_summarize_object_location(){
	
	int i;
	while(ready_to_work == true){		
			
		
			sleep_t(BUSY_WAITING_TIME_IN_MS);
		
	}
	
}

void upload_hashtable_for_all_area(DBConnectionListHead *db_connection_list_head,char *server_installation_path,
									int ready_for_location_history_table){
	int i;
	for(i=0;i<area_table_max_size;i++){		
		if(area_table[i].area_id==NULL) continue;		
		zlog_error(category_debug,"area table id %s",area_table[i].area_id);
		hashtable_go_through_for_summarize(area_table[i].area_hash_ptr);		
		hashtable_go_through_for_get_summary(
			area_table[i].area_hash_ptr,db_connection_list_head,server_installation_path,
			ready_for_location_history_table);
		printf("-----------------------------------------\n");
	}
}
