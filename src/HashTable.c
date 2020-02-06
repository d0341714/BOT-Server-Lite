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
		8000, 0.7, 1.5, 
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
{	
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
	
	pthread_mutex_unlock(ht_mutex);
	//printf("inserting, {%s => %p}\n", key, value);
	//print_HashTable(h_table);
}

void * hashtable_get(HashTable * h_table, void * key, size_t key_len) {
	uint32_t hash_val;
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
	return ret_val;
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
	hash_table_row* row_ptr;
	uint32_t hash_val = h_table->hash(key, key_len);
	uint32_t index = hash_val % h_table->size;
	HNode * curr = h_table->table[index];
	char coordinateX[9];
	char coordinateY[9];
	
	char* lbeacon_uuid;
    char* initial_timestamp_GMT;
    char* final_timestamp_GMT;
    char* battery_voltage;   
    char* rssi;
    char* panic_button;
	
	lbeacon_uuid=value->lbeacon_uuid;
	initial_timestamp_GMT=value->initial_timestamp_GMT;
	final_timestamp_GMT=value->final_timestamp_GMT;
	battery_voltage=value->battery_voltage;
	rssi=value->rssi;
	panic_button=value->panic_button;
	//mp_free(&DataForHashtable_mempool,value);
	/*
	int initial_timestamp_GMT;
	int final_timestamp_GMT;
	char* battery_voltage;
	int rssi;
	*/
   //10 seconds
   //char* rssi;
   /*printf("hashtable_replace_uuid ret\n");
	return 0;
	printf("hashtable_replace_uuid urn\n");*/
	while (curr) {		
		int res = h_table->equal(curr->key, key);
		if (res == 1) {
			printf("exsist MAC address\n");
			//curr->value = value;
			//lbeacon_uuid=value->lbeacon_uuid;
			if(strlen(lbeacon_uuid)!=32) return 0;
			row_ptr=curr->value;
			record_table_size = row_ptr->record_table_size;
			row_ptr->battery=battery_voltage;
			row_ptr->panic_button=panic_button;
			//匹配uuid
			for(i=0;i<record_table_size;i++){
				if(row_ptr->uuid_record_table_ptr[i]!=NULL){
				//if(row_ptr->uuid_table[i].valid==1){
					if(strcmp(lbeacon_uuid,row_ptr->uuid_record_table_ptr[i]->uuid)==0){//跟傳進來的uuid比較依樣
					//if(strcmp(lbeacon_uuid,row_ptr->uuid_table[i].uuid)==0){
					//更新uuid,ini,final,rssi,battery_voltage
					//printf("exsist uuid: %s\n",lbeacon_uuid);
					//存在的
					printf("final_timestamp_GMT:%s,uuid->final_timestamp:%s",final_timestamp_GMT,row_ptr->uuid_record_table_ptr[i]->final_timestamp);
					time_gap=atoi(final_timestamp_GMT)-atoi(row_ptr->uuid_record_table_ptr[i]->final_timestamp);
					if(time_gap>0){
						row_ptr->uuid_record_table_ptr[i]->final_timestamp=final_timestamp_GMT;
						
						/*
						time_gap=1
						>1 補0
						*/
						rssi_array=row_ptr->uuid_record_table_ptr[i]->rssi_array;
						head=row_ptr->uuid_record_table_ptr[i]->head;
						for(j=0;j<time_gap;j++){
							head++;
							if(head==10){
								head=0;
							}								
							rssi_array[head]=0;	
						}
						rssi_array[head]=atoi(rssi);
						row_ptr->uuid_record_table_ptr[i]->head=head;
					}
					
					
					//curr->value_len = value_len;
					
					
					return 1;
					
					}
				}else{//紀錄invalid的位置
					invalid_place=i;
				}
					
				
			}
			//不再裡面,寫入invalid或擴增空間
			if(invalid_place!=-1){		
				if(row_ptr->uuid_record_table_ptr[invalid_place]==NULL){
					row_ptr->uuid_record_table_ptr[invalid_place]=mp_alloc(&uuid_record_table_row_mempool);
				}
				if(row_ptr->uuid_record_table_ptr[invalid_place]==NULL){
					printf("uuid_record_table_row_mempool NOT ENOUGH!!!!!!!!!!!!!!\n");
					break;
				}
				memset(row_ptr->uuid_record_table_ptr[invalid_place],0,sizeof(uuid_record_table_row));
				printf("invalid_place %d new uuid: %s %d \n",invalid_place,lbeacon_uuid,strlen(lbeacon_uuid));
				row_ptr->uuid_record_table_ptr[invalid_place]->uuid=lbeacon_uuid;
				row_ptr->uuid_record_table_ptr[invalid_place]->initial_timestamp=initial_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[invalid_place]->final_timestamp=final_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[invalid_place]->rssi_array[0]=atoi(rssi);
				row_ptr->uuid_record_table_ptr[invalid_place]->head=0;
				memcpy(coordinateX,lbeacon_uuid+12,8);
				coordinateX[9]='\0';
				row_ptr->uuid_record_table_ptr[invalid_place]->coordinateX=atof(coordinateX);				
				memcpy(coordinateY,lbeacon_uuid+24,8);
				coordinateY[9]='\0';
				row_ptr->uuid_record_table_ptr[invalid_place]->coordinateY=atof(coordinateY);
				printf("new uuid finish!!!!\n");
				return 1;
			}else{//擴增
				//晚點改
				printf("new uuid without enough space: %s\n",lbeacon_uuid);
				row_ptr->record_table_size*=2;
				realloc(row_ptr->uuid_record_table_ptr,row_ptr->record_table_size*sizeof(uuid_record_table_row));
				row_ptr->uuid_record_table_ptr[i]=mp_alloc(&uuid_record_table_row_mempool);
				
				memset(row_ptr->uuid_record_table_ptr[i],0,sizeof(uuid_record_table_row));
				row_ptr->uuid_record_table_ptr[i]->uuid=lbeacon_uuid;
				row_ptr->uuid_record_table_ptr[i]->initial_timestamp=initial_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[i]->final_timestamp=final_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[i]->rssi_array[0]=atoi(value->rssi);
				row_ptr->uuid_record_table_ptr[i]->head=0;
				strncpy(coordinateX,lbeacon_uuid+12,8);
				row_ptr->uuid_record_table_ptr[invalid_place]->coordinateX=atof(coordinateX);
				strncpy(coordinateY,lbeacon_uuid+24,8);
				row_ptr->uuid_record_table_ptr[invalid_place]->coordinateY=atof(coordinateY);
				curr->value_len = sizeof(*row_ptr);
				return 1;
			}
						
			
			
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

//panic
/*
void hashtable_go_through_panic(HashTable * h_table) {
	int size = h_table->size;
	HNode ** table = h_table->table;
	PanicMAC* panic_state_ptr;
	int i = 0;
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {
			// callback(curr->key, curr->key_len, curr->value, curr->value_len);
			panic_state_ptr = curr->value;
			//砍到沒更新而且傳過的
			if(panic_state_ptr->valid==0 && panic_state_ptr->send == 1)
				hashtable_remove(h_table,curr->key, curr->key_len);
			//更新且沒傳過的
			if(panic_state_ptr->valid==1 && panic_state_ptr->send==0){
				//update sql成功再改
				panic_state_ptr->valid=0;
				
				
				panic_state_ptr->send=1;
			}
			
			
			
			//printf("'%s' => %p,\n", (char *)curr->key, curr->value);
			curr = curr->next;
		}
	}
}
*/
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
初始化area table
*/
void initial_area_table(void){
	if(MEMORY_POOL_SUCCESS != mp_init( &hash_table_row_mempool, 
                                       sizeof(hash_table_row), 
                                       SLOTS_IN_MEM_POOL_HASH_TABLE_ROW))
    {
        return E_MALLOC;
    }
	
	if(MEMORY_POOL_SUCCESS != mp_init( &uuid_record_table_row_mempool, 
                                       sizeof(uuid_record_table_row), 
                                       SLOTS_IN_MEM_POOL_UUID_RECORD_TABLE_ROW))
    {
        return E_MALLOC;
    }
	
	if(MEMORY_POOL_SUCCESS != mp_init( &DataForHashtable_mempool, 
                                       sizeof(DataForHashtable), 
                                       SLOTS_IN_MEM_POOL_DataForHashtable_TABLE_ROW))
    {
        return E_MALLOC;
    }
	
	area_table_max_size=16;
	area_table_size=0;
	//panic_table
	panic_table = hashtable_new_default(
			equal_string, 
			destroy_nop, 
			destroy_nop
			);
	return;
}

HashTable * hash_table_of_specific_area_id(char* area_id){
	int i;
	int exist=0;
	HashTable * h_table;
	
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
	
	if(area_table_max_size>=area_table_size){
		
		
		area_table[area_table_size].area_id=area_id;
		area_table[area_table_size].area_hash_ptr=h_table;
		area_table_size++;
	}else{
		//resize
		area_table_max_size*=2;
		realloc(area_table,area_table_max_size*sizeof(AreaTable));
		area_table[area_table_size].area_id=area_id;
		area_table[area_table_size].area_hash_ptr=h_table;
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
	/*
    char *pqescape_object_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_rssi = NULL;
    char *pqescape_push_button = NULL;
    char *pqescape_initial_timestamp_GMT = NULL;
    char *pqescape_final_timestamp_GMT = NULL;
	*/
	char area_id[5];
	HashTable * area_table;
	
	DataForHashtable* data_row;
	int i;
	/**/
	//printf("%s %d\n",buf,buf_len);
	
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);
	
	//printf("string:%s %d\n",temp_buf,buf_len);
	/*神秘問題*/
    //data_row->lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
	lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
	//printf("%s\n",saveptr);
	
	//area+uuid
	
	/**/
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
	//printf("lbeacon_timestamp: %s\n",lbeacon_timestamp);
    if(lbeacon_timestamp == NULL){
        return E_API_PROTOCOL_FORMAT;
    }
    lbeacon_timestamp_value = atoi(lbeacon_timestamp);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
	//printf("lbeacon_ip:%s\n",lbeacon_ip);
    /*zlog_debug(category_debug, "lbeacon_uuid=[%s], lbeacon_timestamp=[%s], " \
               "lbeacon_ip=[%s]", lbeacon_uuid, lbeacon_timestamp, lbeacon_ip);*/
	/*printf("lbeacon_uuid=[%s], lbeacon_timestamp=[%d], " \
               "lbeacon_ip=[%s]\n", lbeacon_uuid, lbeacon_timestamp_value, lbeacon_ip);*/

    //SQL_begin_transaction(db);

    while(num_types --){
		/**/
        object_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        object_number = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        zlog_debug(category_debug, "object_type=[%s], object_number=[%s]", 
                   object_type, object_number);
        if(object_number == NULL){
            //SQL_rollback_transaction(db);
            return E_API_PROTOCOL_FORMAT;
        }
        numbers = atoi(object_number);
		//printf("n:%d",numbers);
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

            /* update hash table */
			/*
			mac:
			ini,fin,uuid,rssi,
			*/
			/*
			找area
			*/
			//strncpy(area_id, lbeacon_uuid, 4);
			
			
			data_row=mp_alloc(&DataForHashtable_mempool);
			if(data_row==NULL) {
				printf("=mp_alloc(&DataForHashtable_mempool) fail\n");
				return 0;
			}
			memset(data_row,0,sizeof(DataForHashtable));
			//lock
			pthread_mutex_init( &data_row->list_lock, 0);
			
			
			memcpy(area_id, lbeacon_uuid, 4);
			area_id[5]='\0';
			
			data_row->lbeacon_uuid=lbeacon_uuid;
			data_row->initial_timestamp_GMT=initial_timestamp_GMT;
			data_row->final_timestamp_GMT=final_timestamp_GMT;
			data_row->rssi=rssi;
			data_row->battery_voltage=battery_voltage;
			data_row->panic_button=panic_button;
			//拆解ok
			//printf("12345:%s %s %s %s %s\n",data_row->lbeacon_uuid,data_row->initial_timestamp_GMT,data_row->final_timestamp_GMT,data_row->rssi,data_row->battery_voltage);
			/**/
			//pthread_mutex_lock(&data_row->list_lock);
			area_table=hash_table_of_specific_area_id(area_id);
			
			//printf("area:%s\n",area_id);
			
			hashtable_put_mac_table(area_table, 
							object_mac_address,strlen(object_mac_address), 
							data_row, sizeof(*data_row));	/**/
			//pthread_mutex_unlock(&data_row->list_lock);
			printf("end lock:data_row->%s",data_row->lbeacon_uuid);
			mp_free(&DataForHashtable_mempool,data_row);
			//unlock
			//printf("area end:%s\n",area_id);
			/*
			push_button判斷
			
			放到queue
			*/
			/*
			if(push_button==1){
				printf("SOS %s\n",object_mac_address);
				/*
				char *sql_identify_panic = 
					"UPDATE object_summary_table " \
					"SET panic_violation_timestamp = NOW() " \
					"object_summary_table.mac_address = %s " ;
				
				
			}
            */
        }
    }

    

    return WORK_SUCCESSFULLY;
}
/*
改成計算weight?

*/
int rssi_weight(int * rssi_array,int k,int head){
	
	if(rssi_array[k]==0) return 0;
	//差距的部分還要再補充
	//比較跟之前1筆的差距,差太多就歸零(當成無用資料
	if((k+9)%10!=head)		
		if(rssi_array[(k+9)%10]!=0)
			if(abs(rssi_array[k]-rssi_array[(k+9)%10])>20)
				return 0;
	
	
	//條件判斷weight
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
	uuid_record_table_row* uuid_table;
	int weight;
	float summary_coordinateX_this_turn;
	float summary_coordinateY_this_turn;
	int recently_scanned;
	hash_table_row* table_row;
	//printf("hashtable_go_through_for_summarize\n");
	
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	/*printf("12345:%s %s %s %s %s\n",value->lbeacon_uuid,value->initial_timestamp_GMT,value->final_timestamp_GMT,
									value->rssi,value->battery_voltage);*/
	pthread_mutex_lock(ht_mutex);
	
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {//every mac
			//callback(curr->key, curr->key_len, curr->value, curr->value_len);
			//printf("'%s' => %p,\n", (char *)curr->key, curr->value);
			
			
			table_row = curr->value;
			
			sum_rssi=0;
			avg_rssi=-100;
			valid_rssi_count=0;	
			
			//先找到原本那份算avg
			for(l=0;l<table_row->record_table_size;l++){
				if(table_row->uuid_record_table_ptr[l]!=NULL && 
				   strcmp(table_row->uuid_record_table_ptr[l]->uuid,table_row->summary_uuid)==0){
					    			
					    uuid_table= table_row->uuid_record_table_ptr[l];
						
						for(k=0;k<10;k++){//計算avg
							//uuid_table->rssi_array[k]
							//check is reasonable
							if(rssi_weight(uuid_table->rssi_array,k,uuid_table->head)>0){
								sum_rssi+=uuid_table->rssi_array[k];
								valid_rssi_count++;								
							}
						}
						avg_rssi=sum_rssi/valid_rssi_count;
						table_row->final_timestamp= table_row->uuid_record_table_ptr[l]->final_timestamp;
						table_row->average_rssi=avg_rssi;
						
						printf("%s first average_rssi:%d\n",(char *)curr->key,table_row->average_rssi);
						break;
					
				}
			}
			//沒找到再拿第一個
			j=0;
			
			
			weight_x=0;
			weight_y=0;
			weight_count=0;
			recently_scanned=0;
			
			while(j<table_row->record_table_size){//不同uuid
				sum_rssi=0;
				valid_rssi_count=0;
				uuid_table= table_row->uuid_record_table_ptr[j];
				
				
				if(uuid_table==NULL) {
					j++;
					continue;	
				}
				//把太久的資料刪掉
				/**/
				printf("uuid_table->final_timestamp %s",uuid_table->final_timestamp);
				printf("last time\n",get_clock_time()-10);
				if(atoi(uuid_table->final_timestamp)<(get_clock_time()-10)){
					
					//要lock
					mp_free(&uuid_record_table_row_mempool,uuid_table);
					j++;
					continue;
				} 
				weight_count_for_specific_uuid=0;
				for(k=0;k<10;k++){//計算avg
					//uuid_table->rssi_array[k]
					//check is reasonable
					weight=rssi_weight(uuid_table->rssi_array,k,uuid_table->head);
					if(weight>0){
						sum_rssi+=uuid_table->rssi_array[k];
						weight_count_for_specific_uuid+=weight;
						valid_rssi_count++;
						
						recently_scanned=1;
					}
					printf("%d %d %d\n",k,uuid_table->rssi_array[k],rssi_weight(uuid_table->rssi_array,k,uuid_table->head));
				}
				
				if(valid_rssi_count==0) continue;
				
				printf("sum_rssi=%d valid_rssi_count=%d\n",sum_rssi,valid_rssi_count);
				weight_x+=uuid_table->coordinateX * weight_count_for_specific_uuid;
				weight_y+=uuid_table->coordinateY * weight_count_for_specific_uuid;				
				weight_count+=weight_count_for_specific_uuid;
				if((sum_rssi/valid_rssi_count)-avg_rssi > 5){//取avg大的(以後可能改成沒差多少的
					table_row->summary_uuid= uuid_table->uuid;
					table_row->initial_timestamp=uuid_table->initial_timestamp;
					table_row->final_timestamp=uuid_table->final_timestamp;
					table_row->average_rssi=sum_rssi/valid_rssi_count;
				}
				
				j++;
			}
			
			if(weight_count!=0){
				summary_coordinateX_this_turn=weight_x/(float)weight_count;
				summary_coordinateY_this_turn=weight_y/(float)weight_count;
				/*
				比較x,y 差不多就不改
				*/
				
				if(abs(summary_coordinateX_this_turn - table_row->summary_coordinateX)>DRIFT_DISTANCE &&
					abs(summary_coordinateY_this_turn - table_row->summary_coordinateY)>DRIFT_DISTANCE){
					//coordinateX
					table_row->summary_coordinateX=summary_coordinateX_this_turn;
					//coordinateY
					table_row->summary_coordinateY=summary_coordinateY_this_turn;
				}
				table_row->recently_scanned=recently_scanned;
			}			
			
			//every mac
			printf("mac:%s, recently_scanned:%d, summary_coordinateX:%f, final_time:%s\n",
						(char *)curr->key,table_row->recently_scanned,table_row->summary_coordinateX,table_row->final_timestamp);
			
			curr = curr->next;
		}
	}
		pthread_mutex_unlock(ht_mutex);
}

void hashtable_go_through_for_get_summary(
		HashTable * h_table,DBConnectionListHead *db_connection_list_head,char *server_installation_path) {
	int size = h_table->size;
	HNode ** table = h_table->table;
	char* uuid;
    char* initial_timestamp;
    char* final_timestamp;
	char* battery;
	char* panic_button;
	char* average_rssi;
	int recently_scanned;
	int i = 0;
	hash_table_row* table_row;
	char *pqescape_mac_address = NULL;
	PGconn *db_conn = NULL;
	int db_serial_id = -1;
	char* coordinate_x;
	char* coordinate_y;
	
	char *sql_identify_panic = 
        "UPDATE object_summary_table " \
        "SET rssi = %s,first_seen_timestamp = %s ," \
        "last_seen_timestamp = %s, battery_voltage = %s , "\
		"uuid = %s"\
		"base_x = %s, base_y = %s , "\
		"SET panic_violation_timestamp = NOW() " \
        "WHERE object_summary_table.mac_address = %s " ;
    char *sql_without_panic =
		"UPDATE object_summary_table " \
        "SET rssi = %s,first_seen_timestamp = %s ," \
        "last_seen_timestamp = %s, battery_voltage = %s , "\
		"uuid = %s"\
		"base_x = %s, base_y = %s , "\
        "WHERE object_summary_table.mac_address = %s " ;
	char sql[SQL_TEMP_BUFFER_LENGTH];
	printf("hashtable_go_through_for_get_summary\n");
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {
			// callback(curr->key, curr->key_len, curr->value, curr->value_len);
			printf("'%s' => %p,\n", (char *)curr->key, curr->value);
			
			table_row = curr->value;
			//取下面四個,update db
			if(table_row==NULL) continue;
			printf("NOT NULL\n");
			if(table_row->recently_scanned==NULL) continue;
			printf("recently_scanned NOT NULL\n");
			if(table_row->recently_scanned>0){
				
				uuid=table_row->summary_uuid;
				battery=table_row->battery;
				initial_timestamp=table_row->initial_timestamp;
				final_timestamp=table_row->final_timestamp;
				average_rssi=itoa(table_row->average_rssi);
				panic_button=table_row->panic_button;
				coordinate_x=itoa((int)summary_coordinateX);
				coordinate_y=itoa((int)summary_coordinateY);
				//印出來看看?
				printf("summary:%s %s %s %s %s %s\n",uuid,battery,initial_timestamp,final_timestamp,average_rssi,panic_button);
				printf("x+y:%s %s\n",coordinate_x,coordinate_y);
				//上傳db
				//panic
				 memset(sql, 0, sizeof(sql));
				 if(WORK_SUCCESSFULLY != 
                   SQL_get_database_connection(db_connection_list_head, 
                                               &db_conn, 
                                               &db_serial_id)){

                    zlog_error(category_debug,
                               "cannot open database\n");

                    continue;
                }

                pqescape_mac_address = 
                    PQescapeLiteral(db_conn, object_mac_address, 
                                    strlen(object_mac_address)); 
   
                

                
				}
				if(panic_button!=NULL && atoi(panic_button)==1){
					//字串不同
					sprintf(sql,sql_without_panic, 
							average_rssi,initial_timestamp,
							final_timestamp,battery,
							uuid,
							coordinate_x,coordinate_y,
							pqescape_mac_address);
				}else{
					sprintf(sql,sql_without_panic,
							average_rssi,initial_timestamp,
							final_timestamp,battery,
							uuid,
							coordinate_x,coordinate_y,
							pqescape_mac_address);
				}
				PQfreemem(pqescape_mac_address);

                ret_val = SQL_execute(db_conn, sql);

                SQL_release_database_connection(
                    db_connection_list_head, 
                    db_serial_id);
				
			}
			
			printf("crash?\n");
			curr = curr->next;
		}
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
	hash_table_row* hash_table_row_ptr;
	uuid_record_table_row uuid_table;
	char coordinateX[9];
	char coordinateY[9];
	char* lbeacon_uuid;
	char* initial_timestamp_GMT;
	char* final_timestamp_GMT;
	char* battery_voltage;
	char* rssi;
	char* panic_button;
	DataForHashtable* data_row;
	//try to replace existing key's value if possible
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	/*printf("12345:%s %s %s %s %s\n",value->lbeacon_uuid,value->initial_timestamp_GMT,value->final_timestamp_GMT,
									value->rssi,value->battery_voltage);*/
	pthread_mutex_lock(ht_mutex);
	//printf("==>hashtable_put_mac_table\n");
	/*printf("12345:%s %s %s %s %s\n",value->lbeacon_uuid,value->initial_timestamp_GMT,value->final_timestamp_GMT,
									value->rssi,value->battery_voltage);*/
	/*避免value本來就錯
	if(strlen(value->lbeacon_uuid)!=32||value->initial_timestamp_GMT==NULL||value->final_timestamp_GMT==NULL
		||value->battery_voltage==NULL||value->rssi==NULL||value->panic_button==NULL){
		printf("error value\n");
		mp_free(&DataForHashtable_mempool,value);
		return 0;
	}else{}*/
	//printf("2222\n");
	lbeacon_uuid=value->lbeacon_uuid;
	initial_timestamp_GMT=value->initial_timestamp_GMT;
	final_timestamp_GMT=value->final_timestamp_GMT;
	battery_voltage=value->battery_voltage;
	rssi=value->rssi;
	panic_button=value->panic_button;
	
	//printf("11111\n");
	//mp_free(&DataForHashtable_mempool,value);	
	
	//printf("hashtable_put_mac_table\n");
	
	
	
	//replace 改成可以用的
	/*data_row=mp_alloc(&DataForHashtable_mempool);
	if(data_row==NULL) {
		printf("=mp_alloc(&DataForHashtable_mempool) fail\n");
		pthread_mutex_unlock(ht_mutex);
		return 0;
	}
	memset(data_row,0,sizeof(DataForHashtable));
	//printf("lbeacon~~~\n");
	data_row->lbeacon_uuid=lbeacon_uuid;
	data_row->initial_timestamp_GMT=initial_timestamp_GMT;
	data_row->final_timestamp_GMT=final_timestamp_GMT;
	//printf("lbeacon11111~~~\n");
	data_row->rssi=rssi;
	data_row->battery_voltage=battery_voltage;
	//printf("lbeacon22222~~~\n");
	data_row->panic_button=panic_button;
	*/
	//printf("==>hashtable_replace_uuid\n");
	
	res = _hashtable_replace_uuid(h_table, key, key_len, value, sizeof(*value));
	
	//printf("replace finish\n");
	//找不到要新增一個
	if (res == 0 ) {
		
		//  resize if needed. 
		double load = ((double) h_table->count) / ((double) h_table->size);
		if (load > h_table->max_load) {
			pthread_mutex_unlock(ht_mutex);
			return;
			printf("resize!!!!!!!!!!!!!! %f %f\n",load,h_table->max_load);
			_hashtable_resize(h_table);
		}
		
		printf("new %s\n",key);
		//code to add key and value in O(1) time.
		hash_val = h_table->hash(key, key_len);
		index = hash_val % h_table->size;
		prev_head = h_table->table[index];
		
		
		
		/*
		new_head = hnode_new(
			key, key_len,
			value, value_len,
			h_table->deleteKey,
			h_table->deleteValue
		);
		*/
		new_head = malloc(sizeof(HNode));
		if (new_head != 0 ) {
			new_head->key = key;
			new_head->key_len = key_len;
			//生一個DataForHashtable
			
			//hn->value = value;			
			//new_head->value_len = value_len;
			
			hash_table_row_ptr=mp_alloc(&hash_table_row_mempool);
			memset(hash_table_row_ptr,0,sizeof(hash_table_row));
			hash_table_row_ptr->summary_uuid=lbeacon_uuid;
			hash_table_row_ptr->panic_button=panic_button;
			printf("value->lbeacon_uuid: %s\n",lbeacon_uuid);			
			//strncpy(coordinateX,(value->lbeacon_uuid)+12,8);
			memcpy(coordinateX,(lbeacon_uuid)+12,8);
			coordinateX[8]='\0';
			
			hash_table_row_ptr->summary_coordinateX =atof(coordinateX);
			//printf("%f\n",hash_table_row_ptr->summary_coordinateX);
			memcpy(coordinateY,lbeacon_uuid+24,8);
			coordinateY[8]='\0';	
			hash_table_row_ptr->summary_coordinateY=atof(coordinateY);
			printf("X:%f,Y:%f\n",hash_table_row_ptr->summary_coordinateX,hash_table_row_ptr->summary_coordinateY);
			/*
			hash_table_row_ptr->summary_coordinateX=0;
			hash_table_row_ptr->summary_coordinateY=0;
			*/
			hash_table_row_ptr->battery=battery_voltage;
			hash_table_row_ptr->average_rssi=-100;//這邊在想一下
			hash_table_row_ptr->initial_timestamp=initial_timestamp_GMT;
			hash_table_row_ptr->final_timestamp=final_timestamp_GMT;
			
			hash_table_row_ptr->recently_scanned=0;
			
			//uuid_table=hash_table_row_ptr->uuid_table[0];
			
			if(hash_table_row_ptr->uuid_record_table_ptr[0]==NULL){
				hash_table_row_ptr->uuid_record_table_ptr[0]=mp_alloc(&uuid_record_table_row_mempool);
				memset(hash_table_row_ptr->uuid_record_table_ptr[0],0,sizeof(uuid_record_table_row));
				hash_table_row_ptr->uuid_record_table_ptr[0]->uuid = lbeacon_uuid;				
				hash_table_row_ptr->uuid_record_table_ptr[0]->initial_timestamp = initial_timestamp_GMT;
				hash_table_row_ptr->uuid_record_table_ptr[0]->final_timestamp = final_timestamp_GMT;
				hash_table_row_ptr->uuid_record_table_ptr[0]->head=0;
				//printf("%s",value->rssi);
				hash_table_row_ptr->uuid_record_table_ptr[0]->rssi_array[0]=atoi(rssi);
				/* Construct UUID as aaaa00zz0000xxxxxxxx0000yyyyyyyy format to represent 
									 1			13-20		25-32
				   lbeacon location. In the UUID format, aaaa stands for the area id, zz 
				   stands for the floor level with lowest_basement_level adjustment, 
				   xxxxxxxx stands for the relative longitude, and yyyyyyyy stands for
				   the relative latitude.
				*/
				printf("lbeacon_uuid:%s  \n",lbeacon_uuid);
				memcpy(coordinateX,lbeacon_uuid+12,8);
				coordinateX[8]='\0';
				//printf("coordinateX:%s\n",coordinateX);
				hash_table_row_ptr->uuid_record_table_ptr[0]->coordinateX =atof(coordinateX);
				
				memcpy(coordinateY,lbeacon_uuid+24,8);
				coordinateY[8]='\0';
				hash_table_row_ptr->uuid_record_table_ptr[0]->coordinateY=atof(coordinateY);
			}
			/**/
			
			//uuid_table_ptr=hash_table_row_ptr->uuid_record_table_ptr0;
			/*
			uuid_table.valid=1;
			uuid_table.uuid = value->lbeacon_uuid;
			uuid_table.initial_timestamp = value->initial_timestamp_GMT;
			uuid_table.final_timestamp = value->final_timestamp_GMT;
			uuid_table.head=0;
			uuid_table.rssi_array[0]=value->rssi;			
			*/

			hash_table_row_ptr->record_table_size=16;
			//sizeof?????
			new_head->value = hash_table_row_ptr;
			new_head->value_len=sizeof(*hash_table_row_ptr);
			
			
			new_head->deleteKey = h_table->deleteKey;
			new_head->deleteValue = h_table->deleteValue;
			/**/

		}
	
		new_head->next = prev_head;
		h_table->table[index] = new_head;
		h_table->count = h_table->count + 1;
	}
	
	pthread_mutex_unlock(ht_mutex);
	printf("<==hashtable_put_mac_table\n");
	//printf("inserting, {%s => %p}\n", key, value);
	//print_HashTable(h_table);
}
/*
	每秒
	跑過全部的hashtable
*/	
void hashtable_summarize_object_location(){
	
	int i;
	while(ready_to_work == true){
		//??
		
			/*for(i=0;i<area_table_max_size;i++){
				if(area_table[i].area_id==NULL) continue;
				//printf("1234:%s",);
				hashtable_go_through_for_summarize(area_table[i].area_hash_ptr);
				
			}*/
			
		
			sleep_t(BUSY_WAITING_TIME_IN_MS);
		
	}
	
}

void upload_hashtable_for_all_area(DBConnectionListHead *db_connection_list_head,char *server_installation_path){
	int i;
	for(i=0;i<area_table_max_size;i++){
		if(area_table[i].area_id==NULL) continue;		
		/*
		printf("hashtable_go_through_for_summarize %s",area_table[i].area_id);
		hashtable_go_through_for_summarize(area_table[i].area_hash_ptr);
		
		printf("hashtable_go_through_for_get_summary %s",area_table[i].area_id);
		hashtable_go_through_for_get_summary(
			area_table[i].area_hash_ptr,db_connection_list_head,server_installation_path);*/
	}
}
