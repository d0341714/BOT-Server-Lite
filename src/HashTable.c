#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#include "HashTable.h"

// private interface

static uint32_t _hashtable_hash_adler32(const void *buf, size_t buflength);

static int _hashtable_replace(HashTable * h_table, 
                              void * key, 
                              size_t key_len, 
                              void * value, 
                              size_t value_len);

static int _hashtable_replace_uuid(HashTable * h_table, 
                                   void * key, 
                                   size_t key_len, 
                                   void * value, 
                                   int number_of_rssi_signals_under_tracked);

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
        8000, 1, 1, 
        equal, _hashtable_hash_adler32, 
        deleteKey, deleteValue
    );

}

static int _hashtable_replace_uuid(
    HashTable * h_table, 
    void * key, 
    size_t key_len, 
    DataForHashtable * value, 
    int number_of_rssi_signals_under_tracked
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
    uuid_record_table_row* uuid_record_table_row_resize_ptr;
        
    while (curr) {      
        int res = h_table->equal(curr->key, key);
        if (res == 1) {         
            exist_MAC_address_row=curr->value;
            record_table_size = exist_MAC_address_row->record_table_size;
            strcpy(exist_MAC_address_row->battery,value->battery_voltage);
            strcpy(exist_MAC_address_row->panic_button,value->panic_button);
            strcpy(exist_MAC_address_row->final_timestamp,value->final_timestamp_GMT);
            
            //match uuid
            for(i=0;i<record_table_size;i++){
                if(exist_MAC_address_row->uuid_record_table_array[i].valid==1){
                    if(0 == strcmp(value->lbeacon_uuid,
                                   exist_MAC_address_row -> 
                                   uuid_record_table_array[i].uuid)){
                    
                    time_gap = atoi(value -> final_timestamp_GMT) - 
                               atoi(exist_MAC_address_row -> 
                               uuid_record_table_array[i].final_timestamp);

                    if(time_gap > 0){

                        strcpy(exist_MAC_address_row -> 
                               uuid_record_table_array[i].final_timestamp, 
                               value->final_timestamp_GMT);
                        
                        head = exist_MAC_address_row -> uuid_record_table_array[i].head;

                        for(j=0;j<time_gap;j++){

                            head++;
                            if(head == number_of_rssi_signals_under_tracked){
                                head=0;
                            }

                            exist_MAC_address_row -> 
                                uuid_record_table_array[i].rssi_array[head] = 0;    
                        }   
                        exist_MAC_address_row -> 
                        uuid_record_table_array[i].rssi_array[head] = value->rssi;

                        exist_MAC_address_row -> 
                        uuid_record_table_array[i].head = head;
                    }           
                                    
                    return 1;
                    
                    }
                }else{
                    invalid_place=i;
                }
                    
                
            }
            
            //new uuid
            if(invalid_place!=-1){
                
                strcpy(exist_MAC_address_row -> 
                       uuid_record_table_array[invalid_place].uuid,
                       value->lbeacon_uuid);

                strcpy(exist_MAC_address_row -> 
                       uuid_record_table_array[invalid_place].initial_timestamp, 
                       value -> initial_timestamp_GMT);

                strcpy(exist_MAC_address_row -> 
                       uuid_record_table_array[invalid_place].final_timestamp, 
                       value->final_timestamp_GMT);

                exist_MAC_address_row -> 
                uuid_record_table_array[invalid_place].rssi_array[0] = 
                value->rssi;

                exist_MAC_address_row -> 
                uuid_record_table_array[invalid_place].head = 0;

                memcpy(coordinateX,value -> lbeacon_uuid+12,8);
                coordinateX[8]='\0';
                
                exist_MAC_address_row -> 
                uuid_record_table_array[invalid_place].coordinateX = 
                atof(coordinateX);          

                memcpy(coordinateY,value -> lbeacon_uuid+24,8);
                coordinateY[8]='\0';

                exist_MAC_address_row -> 
                uuid_record_table_array[invalid_place].coordinateY = 
                atof(coordinateY);
                
                exist_MAC_address_row -> 
                uuid_record_table_array[invalid_place].valid = 1;

                curr -> value = exist_MAC_address_row;
                curr -> value_len = sizeof(exist_MAC_address_row);
                
            }else{
                zlog_error(category_debug,"need more uuid record table");
                return 1;               
            }                   
            
            return 1;
        }
        curr = curr->next;
    }
    //不存在
    return 0;
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
void initial_area_table(int rssi_weight_parameter,int drift_distance,int drift_rssi){
    
    int i;
    zlog_debug(category_debug,">>initial_area_table");

    if(MEMORY_POOL_SUCCESS != mp_init( &hash_table_row_mempool, 
                                       sizeof(hash_table_row), 
                                       SLOTS_IN_MEM_POOL_HASH_TABLE_NODE))
    {
        zlog_error(category_debug,"initial fail hash_table_row_mempool");
        return E_MALLOC;
    }
    
    if(MEMORY_POOL_SUCCESS != mp_init( &mac_address_mempool, 
                                       LENGTH_OF_MAC_ADDRESS, 
                                       SLOTS_IN_MEM_POOL_MAC_ADDRESS))
    {
        zlog_error(category_debug,"initial fail mac_address_mempool");
        return E_MALLOC;
    }

    area_table_max_size=INITIAL_AREA_TABLE_MAX_SIZE;
    area_table_size=0;
    //area_table= NULL;
    area_table = malloc((area_table_max_size * sizeof(AreaTable)));
    for(i=0;i<area_table_max_size;i++){
        area_table[i].area_id = 0;
        area_table[i].area_hash_ptr=NULL;
    }/**/
    pthread_mutex_init( &area_table_lock, 0);
    zlog_debug(category_debug,"initial_area_table successful"); 
    return;
}

HashTable * hash_table_of_specific_area_id(int area_id){
    
    int i;
    HashTable * h_table;
    AreaTable* area_table_resize_ptr;
    
    pthread_mutex_lock(&area_table_lock);

    zlog_debug(category_debug,"area id %d",area_id);
    
    for(i=0;i<area_table_max_size;i++){
        if(area_table[i].area_id == 0)
            continue;

        if(area_table[i].area_id == area_id){
            pthread_mutex_unlock(&area_table_lock);
            return area_table[i].area_hash_ptr;
        }
    }
    //not exsist
    h_table = hashtable_new_default(
        equal_string, 
        destroy_nop, 
        destroy_nop
        );
    
    if(area_table_max_size>area_table_size){

        area_table[area_table_size].area_id = area_id;
        area_table[area_table_size].area_hash_ptr=h_table;
        
        area_table_size++;
    }else{
        //resize
        area_table_max_size*=2;
        area_table_resize_ptr=realloc(area_table,area_table_max_size*sizeof(AreaTable));
        if(area_table_resize_ptr==NULL){
            zlog_error(category_debug,"area_table_resize_ptr==null");           
        }
        area_table = area_table_resize_ptr;
        area_table[area_table_size].area_id = area_id;
        area_table[area_table_size].area_hash_ptr=h_table;
        
        area_table_size++;  
        //initial
        for(i=area_table_size;i<area_table_max_size;i++){
            area_table[i].area_id = 0;
        }
         
    }
    pthread_mutex_unlock(&area_table_lock);
    return h_table;
}

ErrorCode hashtable_update_object_tracking_data(
    DBConnectionListHead *db_connection_list_head,
    char* buf,
    size_t buf_len,
    int number_of_lbeacons_under_tracked,
    int number_of_rssi_signals_under_tracked){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    
    int num_types = 2; // BR_EDR and BLE types
    /**/
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;   
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
    char str_area_id[5];
    int area_id = 0;
    HashTable * area_table_ptr;
    
    DataForHashtable data_row;
    int i;
    
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);
    
    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);   
    lbeacon_timestamp_value = atoi(lbeacon_timestamp);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    
    strncpy(str_area_id, lbeacon_uuid, LENGTH_OF_AREA_ID - 1);
    area_id = atoi(str_area_id);
    area_table_ptr=hash_table_of_specific_area_id(area_id);
    
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

            if(atoi(panic_button)){
                 SQL_identify_panic_status(db_connection_list_head,  
                                           object_mac_address);
            }
            
            memset(&data_row, 0, sizeof(DataForHashtable));
            
            strcpy(data_row.lbeacon_uuid,lbeacon_uuid);
            strcpy(data_row.initial_timestamp_GMT,initial_timestamp_GMT);
            strcpy(data_row.final_timestamp_GMT,final_timestamp_GMT);
            data_row.rssi=atoi(rssi);
            strcpy(data_row.battery_voltage,battery_voltage);
            strcpy(data_row.panic_button,panic_button);    
            
            
            hashtable_put_mac_table(area_table_ptr, 
                                    object_mac_address,
                                    LENGTH_OF_MAC_ADDRESS, 
                                    &data_row, 
                                    number_of_lbeacons_under_tracked,
                                    number_of_rssi_signals_under_tracked);    
        }
    }

    
    return WORK_SUCCESSFULLY;
}

    
void hashtable_put_mac_table(HashTable * h_table, 
                             void * key, 
                             size_t key_len, 
                             DataForHashtable * value, 
                             int number_of_lbeacons_under_tracked,
                             int number_of_rssi_signals_under_tracked) 
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
    res = _hashtable_replace_uuid(h_table, key, key_len, value, number_of_rssi_signals_under_tracked);
    //for a new uuid
    if (res == 0 ) {
                    
        //code to add key and value in O(1) time.
        hash_val = h_table->hash(key, key_len);
        index = hash_val % h_table->size;       
        prev_head = h_table->table[index];  
        
        new_head = malloc(sizeof(HNode));
        memset(new_head,0,sizeof(HNode));
        if (new_head != 0 ) {           
            MAC_address=mp_alloc(&mac_address_mempool);
            memset(MAC_address,0,LENGTH_OF_MAC_ADDRESS);
            strcpy(MAC_address,key);
            
            hash_table_row_for_new_MAC=mp_alloc(&hash_table_row_mempool);
            memset(hash_table_row_for_new_MAC,0,sizeof(hash_table_row));
            
            strcpy(hash_table_row_for_new_MAC->summary_uuid , value->lbeacon_uuid);
            strcpy(hash_table_row_for_new_MAC->panic_button,value->panic_button);   
            /* Construct UUID as aaaa00zz0000xxxxxxxx0000yyyyyyyy format to represent 
                                     1          13-20       25-32
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
            hash_table_row_for_new_MAC->uuid_record_table_array[0].rssi_array[0]=value->rssi;           
            hash_table_row_for_new_MAC->uuid_record_table_array[0].coordinateX =atof(coordinateX);          
            hash_table_row_for_new_MAC->uuid_record_table_array[0].coordinateY=atof(coordinateY);

            for(i=1;i<MAX_NUMBER_OF_LBEACON_UNDER_TRACKING;i++){
                hash_table_row_for_new_MAC->uuid_record_table_array[i].valid=0;
            }

            hash_table_row_for_new_MAC -> record_table_size = 
                number_of_lbeacons_under_tracked;

            new_head->key = MAC_address;
            new_head->key_len =LENGTH_OF_MAC_ADDRESS;
            
            new_head->value = hash_table_row_for_new_MAC;
            new_head->value_len=sizeof(hash_table_row_for_new_MAC);         
            
            new_head->deleteKey = h_table->deleteKey;
            new_head->deleteValue = h_table->deleteValue;
            new_head->next=0;
            /**/
            new_head->next = prev_head;
            
            h_table->count = h_table->count + 1;
            h_table->table[index] = new_head;                       

        }
                
    }
    
    pthread_mutex_unlock(ht_mutex);
    
}

int get_rssi_weight(int * rssi_array, 
                    int k,
                    int head, 
                    int rssi_weight_multiplier, 
                    int unreasonable_rssi_change, 
                    int number_of_rssi_signals_under_tracked){
    
    if(rssi_array[k]==0) 
        return 0;

    //Ignore the rssi signal at index k of rssi_array, if its rssi signal 
    //is dramatically compared to the rssi signal at index of k-1
    if((k + number_of_rssi_signals_under_tracked - 1) % 
        number_of_rssi_signals_under_tracked != head ){     
    
        // Check if k-1 is a valid index
        if(rssi_array[(k + number_of_rssi_signals_under_tracked - 1 ) 
           % number_of_rssi_signals_under_tracked ] != 0 ){

            if(abs(rssi_array[k] - 
               rssi_array[( k + number_of_rssi_signals_under_tracked - 1 ) 
               % number_of_rssi_signals_under_tracked ]) > 
               unreasonable_rssi_change ){

                return 0;

            }
        }
    }

    //return the corresponding weight according to the rssi singal at index k
    //of rssi_array
    if(rssi_array[k]>-50)
        return pow(rssi_weight_multiplier,7);
    else if(rssi_array[k]>-55)
        return pow(rssi_weight_multiplier,6);
    else if(rssi_array[k]>-60)
        return pow(rssi_weight_multiplier,5);
    else if(rssi_array[k]>-65)
        return pow(rssi_weight_multiplier,4);
    else if(rssi_array[k]>-70)
        return pow(rssi_weight_multiplier,3);
    else if(rssi_array[k]>-80)
        return pow(rssi_weight_multiplier,2);
    else if(rssi_array[k]>-90)
        return pow(rssi_weight_multiplier,1);
    else if(rssi_array[k]>=-100)
        return 1;
    return 0;
}

//go through
void hashtable_go_through_for_summarize(
    HashTable * h_table,
    int number_of_rssi_signals_under_tracked,
    int unreasonable_rssi_change,
    int rssi_weight_multiplier,
    int hashtable_go_through_for_summarize,
    int drift_distance) {

    int size = h_table->size;
    HNode ** table = h_table->table;
    int i = 0;
    int j=0;
    int k=0;
    int m=0;
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
            for(m=0; m < table_row->record_table_size; m++){
                if(table_row->uuid_record_table_array[m].valid!=0 &&
                   strcmp(table_row->uuid_record_table_array[m].uuid,table_row->summary_uuid)==0){
                        if(atoi(table_row->uuid_record_table_array[m].final_timestamp)<(get_clock_time()-10)){                          
                            break;
                        }
                        for(k=0;k < number_of_rssi_signals_under_tracked;k++){
                            if(get_rssi_weight(table_row->uuid_record_table_array[m].rssi_array,
                                               k,
                                               table_row->uuid_record_table_array[m].head, 
                                               rssi_weight_multiplier,
                                               unreasonable_rssi_change,
                                               number_of_rssi_signals_under_tracked)>0){

                                sum_rssi+=table_row->uuid_record_table_array[m].rssi_array[k];
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
            
            // traverse all Lbeacon uuid
            while(j < table_row->record_table_size){
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
                for(k=0;k<number_of_rssi_signals_under_tracked;k++){
                    //check is reasonable
                    weight=get_rssi_weight(table_row->uuid_record_table_array[j].rssi_array,
                                           k,
                                           table_row->uuid_record_table_array[j].head,
                                           rssi_weight_multiplier,
                                           unreasonable_rssi_change,
                                           number_of_rssi_signals_under_tracked);
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
                
                if((sum_rssi/valid_rssi_count)-avg_rssi > hashtable_go_through_for_summarize){
                    strcpy(table_row->summary_uuid, table_row->uuid_record_table_array[j].uuid);
                    strcpy(table_row->initial_timestamp,table_row->uuid_record_table_array[j].initial_timestamp);
                    //table_row->final_timestamp=uuid_table->final_timestamp;
                    
                    avg_rssi=sum_rssi/valid_rssi_count;
                    table_row->average_rssi=avg_rssi;
                }
                
                j++;
            }
            
            table_row->recently_scanned=recently_scanned;
            if(weight_count!=0){
                summary_coordinateX_this_turn=weight_x/(float)weight_count;
                summary_coordinateY_this_turn=weight_y/(float)weight_count;
                
                                
                if(abs(summary_coordinateX_this_turn - table_row->summary_coordinateX) > drift_distance || 
                   abs(summary_coordinateY_this_turn - table_row->summary_coordinateY) > drift_distance){
                    //coordinateX
                    table_row->summary_coordinateX=summary_coordinateX_this_turn;
                    //coordinateY
                    table_row->summary_coordinateY=summary_coordinateY_this_turn;
                }               
                
                
            }           
            
            curr->value=table_row;
            curr = curr->next;
        }
    }       
        pthread_mutex_unlock(ht_mutex);
}


    

void hashtable_traverse_all_areas_to_upload_latest_location(
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path,
    int number_of_rssi_signals_under_tracked,
    int unreasonable_rssi_change,
    int rssi_weight_multiplier,
    int rssi_difference_of_location_accuracy_tolerance,
    int drift_distance){

    int i;
    
    for(i=0; i< area_table_max_size; i++){  

        if(area_table[i].area_id == 0)
            continue;

        zlog_debug(category_debug,"area table id %d",
                   area_table[i].area_id);

        hashtable_go_through_for_summarize(area_table[i].area_hash_ptr, 
                                           number_of_rssi_signals_under_tracked,
                                           unreasonable_rssi_change,
                                           rssi_weight_multiplier,
                                           rssi_difference_of_location_accuracy_tolerance,
                                           drift_distance);

        hashtable_upload_location_to_database(area_table[i].area_hash_ptr,
                                              db_connection_list_head,
                                              server_installation_path,
                                              LATEST_LOCATION_INFO);
    }
    
}

void hashtable_traverse_all_areas_to_upload_history_data(
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path){
    
    int i;
    
    for(i=0; i < area_table_max_size; i++){ 

        if(area_table[i].area_id == 0) 
            continue;

        zlog_debug(category_debug,
                   "hashtable_traverse_all_areas_to_upload_history_data: " \
                   "area table id %d",
                   area_table[i].area_id);    

        hashtable_upload_location_to_database(area_table[i].area_hash_ptr,
                                              db_connection_list_head,
                                              server_installation_path,
                                              LOCATION_FOR_HISTORY);      
    }
}
    
void hashtable_upload_location_to_database(
    HashTable * h_table,
    DBConnectionListHead *db_connection_list_head,
    char *server_installation_path,
    LocationInfoType location_type) {

    int record_size = h_table->size;
    HNode ** table = h_table->table;
    int i = 0;

    char filename[MAX_PATH];
    FILE *file = NULL;
    char location_filename[MAX_PATH];
    FILE *location_file = NULL;
    
    time_t rawtime;
    struct tm ts;
    char buf_initial_time[LENGTH_OF_TIME_FORMAT];
    char buf_final_time[LENGTH_OF_TIME_FORMAT];
    char buf_record_time[LENGTH_OF_TIME_FORMAT];
    int clock_time_now;          
    hash_table_row* table_row;      
    

    if(LOCATION_FOR_HISTORY == location_type){

        sprintf(location_filename, "%s%s_%d", 
                FILE_PREFIX_DUMP_LOCATION_HISTORY_INFORMATION, 
                pthread_self());  

        location_file = fopen(location_filename, "wt");

        if(location_file == NULL){
            zlog_error(category_debug, 
                       "history_table:cannot open filepath %s", 
                       location_filename);
            return ;
        }

    }else{
        sprintf(filename, "%s%s_%d", 
                server_installation_path, 
                FILE_PREFIX_DUMP_LATEST_LOCATION_INFORMATION,
                pthread_self());            

        file = fopen(filename, "wt");
        
        if(file == NULL){
            zlog_error(category_debug, 
                       "track:cannot open filepath %s", 
                       filename);
            return ;
        }
    }
    
    for (i = 0; i < record_size; i++) {

        HNode * curr = table[i];

        while (curr != 0) {    

            table_row = curr->value;  

            if(table_row->recently_scanned > 0){  
            
                zlog_debug(category_debug,
                           "mac %s table_row size:%d",
                           curr->key, sizeof(curr -> key));

                zlog_debug(category_debug,"summary:%s %s %s %s %d %s\n",
                           table_row->summary_uuid,
                           table_row->battery,
                           table_row->initial_timestamp,
                           table_row->final_timestamp,
                           table_row->average_rssi,
                           table_row->panic_button);
                                
                //location history file
                if(LOCATION_FOR_HISTORY == location_type){
                    
                    rawtime = get_system_time();
                    ts = *gmtime(&rawtime);
                    strftime(buf_record_time, sizeof(buf_record_time), 
                            "%Y-%m-%d %H:%M:%S", &ts);
                            
                    fprintf(location_file, "%s,%s,%s,%s,%d,%d,%d\n",
                            curr->key,
                            table_row->summary_uuid,
                            buf_record_time,
                            table_row->battery,
                            table_row->average_rssi,
                            (int)table_row->summary_coordinateX,
                            (int)table_row->summary_coordinateY);
                }else{

                    rawtime = atoi(table_row->initial_timestamp);
                    ts = *gmtime(&rawtime);
                    strftime(buf_initial_time, sizeof(buf_initial_time), 
                             "%Y-%m-%d %H:%M:%S", &ts);
                
                    rawtime = atoi(table_row->final_timestamp);
                    ts = *gmtime(&rawtime);
                    strftime(buf_final_time, sizeof(buf_final_time), 
                             "%Y-%m-%d %H:%M:%S", &ts);
                    
                    fprintf(file, "%s,%d,%s,%s,%s,%d,%d,%s\n",
                            table_row->summary_uuid,
                            table_row->average_rssi,
                            table_row->battery,
                            buf_initial_time,
                            buf_final_time,
                            (int)table_row->summary_coordinateX,
                            (int)table_row->summary_coordinateY,
                            curr->key);
                }
                
            }               
            
            curr = curr->next;
        }
    
    }   
                                   
    if(LOCATION_FOR_HISTORY == location_type){
        fclose(location_file);      
        SQL_upload_location_history(db_connection_list_head,
                                    location_filename);
    }else{
        fclose(file);   
        SQL_upload_hashtable_summarize(db_connection_list_head,
                                       filename);
    }   
}
