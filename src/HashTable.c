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
		1000, 0.7, 1.5, 
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
	char* lbeacon_uuid;
	int record_table_size;
	int invalid_place=-1;
	hash_table_row* row_ptr;
	uint32_t hash_val = h_table->hash(key, key_len);
	uint32_t index = hash_val % h_table->size;
	HNode * curr = h_table->table[index];
	
	/*
	int initial_timestamp_GMT;
	int final_timestamp_GMT;
	char* battery_voltage;
	int rssi;
	*/
   //10 seconds
   //char* rssi;
   
	while (curr) {
		int res = h_table->equal(curr->key, key);
		if (res == 1) {
			
			//curr->value = value;
			lbeacon_uuid=value->lbeacon_uuid;
			row_ptr=(hash_table_row*)curr->value;
			record_table_size = row_ptr->record_table_size;
			row_ptr->battery=value->battery_voltage;
			//匹配uuid
			for(i=0;i<record_table_size;i++){
				if(row_ptr->uuid_record_table_ptr[i]!=NULL){
				//if(row_ptr->uuid_table[i].valid==1){
					if(strcmp(lbeacon_uuid,row_ptr->uuid_record_table_ptr[i]->uuid)==0){//跟傳進來的uuid比較依樣
					//if(strcmp(lbeacon_uuid,row_ptr->uuid_table[i].uuid)==0){
					//更新uuid,ini,final,rssi,battery_voltage
					
					//存在的
					time_gap=atoi(value->final_timestamp_GMT)-atoi(row_ptr->uuid_record_table_ptr[i]->final_timestamp);
					if(time_gap>0){
						row_ptr->uuid_record_table_ptr[i]->final_timestamp=value->final_timestamp_GMT;
						
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
						rssi_array[head]=atoi(value->rssi);
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
				row_ptr->uuid_record_table_ptr[invalid_place]->uuid=lbeacon_uuid;
				row_ptr->uuid_record_table_ptr[invalid_place]->initial_timestamp=value->initial_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[invalid_place]->final_timestamp=value->final_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[invalid_place]->rssi_array[0]=atoi(value->rssi);
				row_ptr->uuid_record_table_ptr[invalid_place]->head=0;
				
			}else{//擴增
				//晚點改
				row_ptr->record_table_size*=2;
				realloc(row_ptr->uuid_record_table_ptr,row_ptr->record_table_size*sizeof(uuid_record_table_row));
				row_ptr->uuid_record_table_ptr[i]=mp_alloc(&uuid_record_table_row_mempool);
				row_ptr->uuid_record_table_ptr[i]->uuid=lbeacon_uuid;
				row_ptr->uuid_record_table_ptr[i]->initial_timestamp=value->initial_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[i]->final_timestamp=value->final_timestamp_GMT;
				row_ptr->uuid_record_table_ptr[i]->rssi_array[0]=atoi(value->rssi);
				row_ptr->uuid_record_table_ptr[i]->head=0;
				
				curr->value_len = sizeof(*row_ptr);
			}
			
			
			
			return 1;
		}
		curr = curr->next;
	}
	//不存在
	return 0;
}

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
    char *push_button = NULL;
    int current_time = get_system_time();
    int lbeacon_timestamp_value;
    char *pqescape_object_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_rssi = NULL;
    char *pqescape_push_button = NULL;
    char *pqescape_initial_timestamp_GMT = NULL;
    char *pqescape_final_timestamp_GMT = NULL;
	
	char *area_id= NULL;
	HashTable * area_table;
	HashTable * panic_table;
	DataForHashtable* data_row;
	int i;
	/**/
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);
	
    data_row->lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
	
	//area+uuid
	strncpy(area_id, buf, 4);
	/**/
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    if(lbeacon_timestamp == NULL){
        return E_API_PROTOCOL_FORMAT;
    }
    lbeacon_timestamp_value = atoi(lbeacon_timestamp);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    zlog_debug(category_debug, "lbeacon_uuid=[%s], lbeacon_timestamp=[%s], " \
               "lbeacon_ip=[%s]", lbeacon_uuid, lbeacon_timestamp, lbeacon_ip);
	

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
		
        while(numbers--){
			
            object_mac_address = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            data_row->initial_timestamp_GMT = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            data_row->final_timestamp_GMT = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            data_row->rssi = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
			push_button = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
            data_row->battery_voltage = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            /* update hash table */
			/*
			mac:
			ini,fin,uuid,rssi,
			*/
			/*
			找area
			*/
			area_table=hash_table_of_specific_area_id(area_id);
			
			/*
			=hashtable_get(area_table, object_mac_address, sizeof(object_mac_address))
			if( == NULL){
				
			}
			*/
			/*
			update資料
			*/
			hashtable_put_mac_table(area_table, 
							object_mac_address,sizeof(object_mac_address), 
							data_row, sizeof(data_row));	
			
			/*
			push_button判斷
			放到queue
			*/
			if(push_button==1){
				//check in list
				PanicMAC* panic_state_ptr;
				panic_state_ptr->valid=1;
				panic_state_ptr->send=0;
				//if(hashtable_get(panic_table, object_mac_address, sizeof(object_mac_address))==NULL){
				//這邊再想一下?
				/*
				1.比較timestamp?
				2.remove的時機?
				3.每1秒上傳一次????
				*/
					/*hashtable_put(panic_table, object_mac_address, sizeof(object_mac_address), 
								panic_state_ptr, sizeof(*panic_state_ptr));*/
				//}
				
				/*
				跑總結上傳sql的時候
				go-through(把01刪掉,改成01)
				
				
				*/
				
			}
            
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
	
	return 1;
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
	int weight_x;
	int weight_y;
	int weight_count;
	int valid_rssi_count;
	uuid_record_table_row* uuid_table;
	
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {//every mac
			// callback(curr->key, curr->key_len, curr->value, curr->value_len);
			//printf("'%s' => %p,\n", (char *)curr->key, curr->value);
			
			
			hash_table_row* table_row = curr->value;
			
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
						
						printf("%d\n",table_row->average_rssi);
						break;
					
				}
			}
			//沒找到再拿第一個
			j=0;
			
			
			
			weight_x=0;
			weight_y=0;
			weight_count=0;
			while(j<table_row->record_table_size){//不同uuid
				sum_rssi=0;
				valid_rssi_count=0;
				uuid_table= table_row->uuid_record_table_ptr[j];
				
				if(uuid_table==NULL) {
					j++;
					continue;	
				}
				//把太久的資料刪掉
				/*
				if(atoi(uuid_table->final_timestamp)<(get_clock_time()-10)){
					printf("delete");
					mp_free(&uuid_record_table_row_mempool,uuid_table);
					j++;
					continue;
				} 
				*/
				for(k=0;k<10;k++){//計算avg
					//uuid_table->rssi_array[k]
					//check is reasonable
					if(rssi_weight(uuid_table->rssi_array,k,uuid_table->head)>0){
						sum_rssi+=uuid_table->rssi_array[k];
						
						valid_rssi_count++;
					}
					printf("%d %d %d\n",k,uuid_table->rssi_array[k],rssi_weight(uuid_table->rssi_array,k,uuid_table->head));
				}
				printf("=%d %d\n",sum_rssi,valid_rssi_count);
				if((sum_rssi/valid_rssi_count)-avg_rssi > 5){//取avg大的(以後可能改成沒差多少的
					table_row->summary_uuid= uuid_table->uuid;
					table_row->initial_timestamp=uuid_table->initial_timestamp;
					table_row->final_timestamp=uuid_table->final_timestamp;
					table_row->average_rssi=sum_rssi/valid_rssi_count;
				}
				
				j++;
			}
			
			/*
			比較x,y 差不多就不改
			*/
			curr = curr->next;
		}
	}
}

void hashtable_go_through_for_get_summary(HashTable * h_table) {
	int size = h_table->size;
	HNode ** table = h_table->table;
	char* uuid;
    char* initial_timestamp;
    char* final_timestamp;
	char* battery;
	int average_rssi;
	int i = 0;
	for (i = 0; i < size; i++) {
		HNode * curr = table[i];
		while (curr != 0) {
			// callback(curr->key, curr->key_len, curr->value, curr->value_len);
			//printf("'%s' => %p,\n", (char *)curr->key, curr->value);
			
			hash_table_row* table_row = curr->value;
			//取下面四個,update db
			uuid=table_row->summary_uuid;
			battery=table_row->battery;
			initial_timestamp=table_row->initial_timestamp;
			final_timestamp=table_row->final_timestamp;
			average_rssi=table_row->average_rssi;
			printf("%s %s %s %s %d",uuid,battery,initial_timestamp,final_timestamp,average_rssi);
			//上傳db
			
			//印出來看看?
			
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
	//try to replace existing key's value if possible
	pthread_mutex_t * ht_mutex = h_table->ht_mutex;
	pthread_mutex_lock(ht_mutex);
	
	//replace 改成可以用的
	res = _hashtable_replace_uuid(h_table, key, key_len, value, value_len);
	
	//找不到要新增一個
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
		
		
		
		/*
		new_head = hnode_new(
			key, key_len,
			value, value_len,
			h_table->deleteKey,
			h_table->deleteValue
		);
		*/
		new_head = malloc(sizeof(HNode));
		if (new_head != 0) {
			new_head->key = key;
			new_head->key_len = key_len;
			//生一個DataForHashtable
			
			//hn->value = value;			
			//new_head->value_len = value_len;
			
			hash_table_row_ptr=mp_alloc(&hash_table_row_mempool);
			hash_table_row_ptr->summary_uuid=value->lbeacon_uuid;
			hash_table_row_ptr->battery=value->battery_voltage;
			hash_table_row_ptr->average_rssi=-100;//這邊在想一下
			hash_table_row_ptr->initial_timestamp=value->initial_timestamp_GMT;
			hash_table_row_ptr->final_timestamp=value->final_timestamp_GMT;
			
			//uuid_table=hash_table_row_ptr->uuid_table[0];
			
			if(hash_table_row_ptr->uuid_record_table_ptr[0]==NULL){
				hash_table_row_ptr->uuid_record_table_ptr[0]=mp_alloc(&uuid_record_table_row_mempool);
				hash_table_row_ptr->uuid_record_table_ptr[0]->uuid = value->lbeacon_uuid;
				hash_table_row_ptr->uuid_record_table_ptr[0]->initial_timestamp = value->initial_timestamp_GMT;
				hash_table_row_ptr->uuid_record_table_ptr[0]->final_timestamp = value->final_timestamp_GMT;
				hash_table_row_ptr->uuid_record_table_ptr[0]->head=0;
				hash_table_row_ptr->uuid_record_table_ptr[0]->rssi_array[0]=atoi(value->rssi);
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
	//printf("inserting, {%s => %p}\n", key, value);
	//print_HashTable(h_table);
}

void upload_all_hashtable(void){
	
}
