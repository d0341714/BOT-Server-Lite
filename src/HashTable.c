#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#include "HashTable.h"

// Static function to be used in this file

static uint32_t _hashtable_hash_adler32(const void *buf, size_t buflength);

/*
static int _hashtable_replace(HashTable * h_table, 
                              void * key, 
                              size_t key_len, 
                              void * value, 
                              size_t value_len);
*/

// Helper function implementation

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

    return hashtable_new(NUMBER_ENTRIES_IN_ONE_HASH_TABLE, 
                         1, 
                         1, 
                         equal, 
                         _hashtable_hash_adler32, 
                         deleteKey, 
                         deleteValue
    );

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

void destroy_key_part(void * key) {
    // nothing need to do
    return;
}

void destroy_value_part(void * value){
    hash_table_row * value_part = (hash_table_row*) value;

    pthread_mutex_unlock(&value_part->node_lock);
    pthread_mutex_destroy(&value_part->node_lock);

    return;
}

/*
initial area table
*/
ErrorCode initialize_area_table(){
    
    int i;
    

    zlog_debug(category_debug,">>initial_area_table");

    if(MEMORY_POOL_SUCCESS != mp_init( &hash_table_node_mempool, 
                                       sizeof(HNode), 
                                       SLOTS_IN_MEM_POOL_HASH_TABLE_NODE))
    {
        zlog_error(category_debug,"initial fail hash_table_node_mempool");
        return E_MALLOC;
    }
    
    if(MEMORY_POOL_SUCCESS != mp_init( &mac_address_mempool, 
                                       LENGTH_OF_MAC_ADDRESS, 
                                       SLOTS_IN_MEM_POOL_MAC_ADDRESS))
    {
        zlog_error(category_debug,"initial fail mac_address_mempool");
        return E_MALLOC;
    }


    if(MEMORY_POOL_SUCCESS != mp_init( &hash_table_value_mempool, 
                                       sizeof(hash_table_row), 
                                       SLOTS_IN_MEM_POOL_HASH_TABLE_VALUE))
    {
        zlog_error(category_debug,"initial fail hash_table_value_mempool");
        return E_MALLOC;
    }
   
    area_table_max_size = INITIAL_AREA_TABLE_MAX_SIZE;
    next_index_area_table = 0;
    
    area_table = malloc((area_table_max_size * sizeof(AreaTable)));
    if(area_table == NULL)
    {
        zlog_error(category_debug,"cannot malloc area_table");
        return E_MALLOC;
    }

    for(i = 0; i < area_table_max_size; i++){
        area_table[i].area_id = 0;
        area_table[i].area_hash_ptr = NULL;
    }

    pthread_mutex_init( &area_table_lock, NULL);

    zlog_debug(category_debug,"initial_area_table successful"); 

    return WORK_SUCCESSFULLY;
}

HashTable * hash_table_of_specific_area_id(int area_id){
    
    int i;
    HashTable * h_table;
    AreaTable* area_table_resize_ptr;
    

    zlog_debug(category_debug,"area id %d",area_id);
    
    // search for existing hashtable for input area_id
    for(i = 0; i < area_table_max_size; i++){
        if(area_table[i].area_id == 0)
            break;

        if(area_table[i].area_id == area_id){
            h_table = area_table[i].area_hash_ptr;

            return h_table;
        }
    }

    // input area_id is not in area_table, so create one new element.
    pthread_mutex_lock(&area_table_lock);

    // search for existing hashtable again to avoid 
    // duplicated creation for the same area_id
    for(i = 0; i < area_table_max_size; i++){
        if(area_table[i].area_id == 0)
            break;

        if(area_table[i].area_id == area_id){
            h_table = area_table[i].area_hash_ptr;

            pthread_mutex_unlock(&area_table_lock);
            return h_table;
        }
    }

    // create new hashtable for input area_id    
    if(next_index_area_table >= area_table_max_size){

        //resize the allocated area_table
        area_table_resize_ptr = 
            realloc(area_table, area_table_max_size * 2 * sizeof(AreaTable));

        if(area_table_resize_ptr == NULL){
            zlog_error(category_debug,"area_table_resize_ptr == null");

            pthread_mutex_unlock(&area_table_lock);
            return NULL;
        }

        area_table_max_size *= 2;
        area_table = area_table_resize_ptr;

        for(i = next_index_area_table ; i < area_table_max_size; i++){
            area_table[i].area_id = 0;
            area_table[i].area_hash_ptr = NULL;
        }        
    }

    h_table = hashtable_new_default(equal_string, 
                                    destroy_key_part, 
                                    destroy_value_part);

    area_table[next_index_area_table].area_id = area_id;
    area_table[next_index_area_table].area_hash_ptr = h_table;

    next_index_area_table++;

    pthread_mutex_unlock(&area_table_lock);
    return h_table;
}

ErrorCode hashtable_update_object_tracking_data(
    DBConnectionListHead *db_connection_list_head,
    char* buf,
    size_t buf_len,
    const int number_of_lbeacons_under_tracked,
    const int number_of_rssi_signals_under_tracked){

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
    
    memset(str_area_id, 0, sizeof(str_area_id));
    strncpy(str_area_id, lbeacon_uuid, LENGTH_OF_AREA_ID_IN_UUID);
    area_id = atoi(str_area_id);
    area_table_ptr = hash_table_of_specific_area_id(area_id);

    if(area_table_ptr == NULL){

        zlog_error(category_debug, "cannot locate hashtable for area_id %d",
                   area_id);
        return E_MALLOC;
    }
    
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
            
            strcpy(data_row.lbeacon_uuid, lbeacon_uuid);
            strcpy(data_row.initial_timestamp_GMT, initial_timestamp_GMT);
            strcpy(data_row.final_timestamp_GMT, final_timestamp_GMT);
            data_row.rssi = atoi(rssi);
            strcpy(data_row.battery_voltage, battery_voltage);
            strcpy(data_row.panic_button, panic_button);    
            
            hashtable_put_new_tracking_data(
                area_table_ptr, 
                object_mac_address,
                LENGTH_OF_MAC_ADDRESS, 
                &data_row, 
                number_of_lbeacons_under_tracked,
                number_of_rssi_signals_under_tracked);    
        }
    }

    return WORK_SUCCESSFULLY;
}

uint32_t hashtable_maintain_key_part(
    HashTable * h_table, 
    void * key, 
    size_t key_len, 
    const int number_of_lbeacons_under_tracked,
    int number_of_rssi_signals_under_tracked){ 
 
    uint32_t ret_index = -1;
    int i = 0;
    int j = 0;
    hash_table_row* hash_table_row_for_new_MAC;
    
    uint32_t hash_val = h_table->hash(key, key_len);
    uint32_t index = hash_val % h_table->size;
    HNode * curr = h_table->table[index];
    HNode * prev_head = NULL;
    HNode * new_head = NULL;

    char *MAC_address = NULL;


    while (curr) {      
        // found existing node of mac_address
        if(1 == h_table->equal(curr->key, key)){
            ret_index = index;
            break;
        }
        curr = curr->next;
    }

    if(ret_index != -1)
        return ret_index;
    
   
    // Not found and need to create new node for input mac_address key

    // code to add key and value in O(1) time.
    hash_val = h_table -> hash(key, key_len);
    prev_head = h_table -> table[index];  

    new_head = mp_alloc(&hash_table_node_mempool);
    if(new_head == NULL){
        zlog_error(category_debug,"malloc failed: HNode");

        return ret_index;
    }
    memset(new_head, 0, sizeof(HNode));
               
    MAC_address = mp_alloc(&mac_address_mempool);
    if(MAC_address == NULL){
        free(new_head); 
        zlog_error(category_debug,"malloc failed: mac_address");

        return ret_index;
    }
    memset(MAC_address, 0, LENGTH_OF_MAC_ADDRESS);
    strcpy(MAC_address, key);
            
    hash_table_row_for_new_MAC = mp_alloc(&hash_table_value_mempool);
    if(hash_table_row_for_new_MAC == NULL){
        free(new_head);
        zlog_error(category_debug, "malloc failed: hashtable value");
        mp_free(&hash_table_value_mempool, MAC_address);

        return;
    }
    memset(hash_table_row_for_new_MAC, 0, sizeof(hash_table_row));

    pthread_mutex_init(&hash_table_row_for_new_MAC -> node_lock, NULL);

    hash_table_row_for_new_MAC->last_reported_timestamp = get_system_time();
            
    for(i = 0; i < number_of_lbeacons_under_tracked; i++){
        hash_table_row_for_new_MAC -> 
            uuid_record_table_array[i].is_in_use = false;
        hash_table_row_for_new_MAC ->
            uuid_record_table_array[i].write_index = 0;
    }

    hash_table_row_for_new_MAC -> number_uuid_size = 
        number_of_lbeacons_under_tracked;

    // for node of hashtable structure
    new_head->key = MAC_address;
    new_head->key_len = LENGTH_OF_MAC_ADDRESS;
            
    new_head->value = hash_table_row_for_new_MAC;
    new_head->value_len = sizeof(hash_table_row_for_new_MAC);         
            
    new_head->deleteKey = h_table->deleteKey;
    new_head->deleteValue = h_table->deleteValue;
    new_head->next = NULL;
        
    new_head->next = prev_head;
            
    h_table->count = h_table->count + 1;
    h_table->table[index] = new_head;   

    return ret_index;

}

void hashtable_put_new_tracking_data(
    HashTable * h_table, 
    const void * key, 
    const size_t key_len, 
    DataForHashtable * value, 
    const int number_of_lbeacons_under_tracked,
    const int number_of_rssi_signals_under_tracked){

    uint32_t index;
    HNode * curr = NULL;
    char coordinateX[LENGTH_OF_COORDINATE];
    char coordinateY[LENGTH_OF_COORDINATE];
    char* MAC_address;
    int i;
    int write_index = 0;
    int time_gap = 0;
    
    int j = 0;
    int index_not_used = 0;
    int record_table_size;
    hash_table_row* hash_table_row_for_new_MAC;
    hash_table_row * exist_MAC_address_row;
    const int MISSED_SINGAL_SINCE_SECONDS = 2;

    //try to replace existing key's value if possible
    pthread_mutex_t * ht_mutex = h_table->ht_mutex; 


    pthread_mutex_lock(ht_mutex);

    index = -1;
    index = hashtable_maintain_key_part(
        h_table, 
        key, 
        key_len, 
        number_of_lbeacons_under_tracked,
        number_of_rssi_signals_under_tracked);
    
    if(index == -1){
        pthread_mutex_unlock(ht_mutex);
        return;
    }

    if(index != -1){
        
        curr = h_table->table[index];

        while(curr){
            if(1 == h_table->equal(curr->key, key)){

                exist_MAC_address_row = curr -> value;

                // lock node of mac_address and release whole hashtable lock
                pthread_mutex_lock(&exist_MAC_address_row -> node_lock);
                pthread_mutex_unlock(ht_mutex);


                // update real-time information 
                record_table_size = exist_MAC_address_row -> number_uuid_size;
                strcpy(exist_MAC_address_row -> battery, 
                       value -> battery_voltage);
                strcpy(exist_MAC_address_row -> panic_button,
                       value->panic_button);
                
                //search lbeacon uuid in the array of recently scanned 
                //lbeacon uuid
                index_not_used = -1;

                for(i = 0; i < record_table_size; i++){

                    if(index_not_used == -1 &&
                       !exist_MAC_address_row ->
                        uuid_record_table_array[i].is_in_use){

                       // record the index of not used space of uuid array for 
                       // not-found case below.
                       index_not_used = i;

                    }else if(exist_MAC_address_row -> 
                             uuid_record_table_array[i].is_in_use && 
                             0 == strcmp(value -> lbeacon_uuid,
                                         exist_MAC_address_row -> 
                                         uuid_record_table_array[i].uuid)){

                        // fill the missing rssi signal as zero.
                        time_gap = atoi(value -> final_timestamp_GMT) - 
                                   atoi(exist_MAC_address_row ->
                                   uuid_record_table_array[i].final_timestamp);

                        strcpy(exist_MAC_address_row ->
                               uuid_record_table_array[i].final_timestamp,
                               value -> final_timestamp_GMT);

                        exist_MAC_address_row -> 
                        uuid_record_table_array[i].last_reported_timestamp = 
                            get_system_time();

                        write_index = exist_MAC_address_row -> 
                            uuid_record_table_array[i].write_index;

                        // fill zero to rssi_array[] for the missing seconds.
                        if(time_gap >= MISSED_SINGAL_SINCE_SECONDS){

                            for(j = MISSED_SINGAL_SINCE_SECONDS ; 
                                j <= time_gap ; j++){
                        
                                write_index++;
                                if(write_index == 
                                   number_of_rssi_signals_under_tracked){

                                    write_index = 0;
                                }

                                exist_MAC_address_row -> 
                                uuid_record_table_array[i].
                                rssi_array[write_index] = 0;
                            }
                        }
                    
                        write_index++;
                        if(write_index == 
                            number_of_rssi_signals_under_tracked){

                            write_index = 0;
                        }
                   
                        exist_MAC_address_row ->
                        uuid_record_table_array[i].
                        rssi_array[write_index] = value->rssi;

                        exist_MAC_address_row ->
                        uuid_record_table_array[i].write_index = write_index;
 
                        pthread_mutex_unlock(&exist_MAC_address_row -> node_lock);
                        return;
                    } // else
                } // for-loop
            
                // case of new lbeacon uuid
                if(index_not_used != -1){
                
                    strcpy(exist_MAC_address_row -> 
                           uuid_record_table_array[index_not_used].uuid,
                           value->lbeacon_uuid);

                    strcpy(exist_MAC_address_row -> 
                           uuid_record_table_array[index_not_used].
                           initial_timestamp, 
                           value -> initial_timestamp_GMT);

                    strcpy(exist_MAC_address_row -> 
                           uuid_record_table_array[index_not_used].
                           final_timestamp, 
                           value->final_timestamp_GMT);

                    exist_MAC_address_row ->
                    uuid_record_table_array[index_not_used].
                    last_reported_timestamp  = get_system_time();

                    write_index = 0;
                    exist_MAC_address_row -> 
                    uuid_record_table_array[index_not_used].
                    rssi_array[write_index] = value->rssi;

                    exist_MAC_address_row -> 
                    uuid_record_table_array[index_not_used].
                    write_index = write_index;
                
                    memcpy(coordinateX,value -> 
                           lbeacon_uuid + INDEX_OF_COORDINATE_X_IN_UUID, 
                           LENGTH_OF_COORDINATE_IN_UUID);
                    coordinateX[LENGTH_OF_COORDINATE_IN_UUID]='\0';
                
                    exist_MAC_address_row -> 
                    uuid_record_table_array[index_not_used].
                    coordinateX = atof(coordinateX);          

                    memcpy(coordinateY,value -> 
                           lbeacon_uuid + INDEX_OF_COORDINATE_Y_IN_UUID, 
                           LENGTH_OF_COORDINATE_IN_UUID);
                    coordinateY[LENGTH_OF_COORDINATE_IN_UUID]='\0';

                    exist_MAC_address_row -> 
                    uuid_record_table_array[index_not_used].
                    coordinateY = atof(coordinateY);
                
                    exist_MAC_address_row -> 
                    uuid_record_table_array[index_not_used].is_in_use = true;
                
                }else{
                    zlog_error(category_debug,"need more uuid record table");
                } 

                pthread_mutex_unlock(&exist_MAC_address_row->node_lock);

                break;

            } // if

            curr = curr->next;
        } // while
    }

    pthread_mutex_unlock(ht_mutex);
    return;
}

int get_rssi_weight(float average_rssi,
                    const int rssi_weight_multiplier){
    
    //return the corresponding weight according to the rssi singal 
    if(average_rssi > -40)
        return pow(rssi_weight_multiplier, 9);
    else if(average_rssi > -45)
        return pow(rssi_weight_multiplier, 8);
    else if(average_rssi > -50)
        return pow(rssi_weight_multiplier, 7);
    else if(average_rssi > -55)
        return pow(rssi_weight_multiplier, 6);
    else if(average_rssi > -60)
        return pow(rssi_weight_multiplier, 5);
    else if(average_rssi > -65)
        return pow(rssi_weight_multiplier, 4);
    else if(average_rssi > -70)
        return pow(rssi_weight_multiplier, 3);
    else if(average_rssi > -80)
        return pow(rssi_weight_multiplier, 2);
    else if(average_rssi > -90)
        return pow(rssi_weight_multiplier, 1);
    else if(average_rssi >= -100)
        return 1;
    return 0;
}

int get_average_rssi(const int *rssi_array,
                     const int rssi_threashold_for_summarize_location_pin,
                     const int number_of_rssi_signals_under_tracked,
                     const int unreasonable_rssi_change){

    int k = 0;
    int valid_rssi_count = 0;
    int prev_index = 0;
    int sum_rssi = 0;
    int ret_avg_rssi = 0;

    for(k = 0; k < number_of_rssi_signals_under_tracked; k++){

        // ignore the timing at which no rssi signal was scanned
        if(rssi_array[k] == 0)
            continue;
   
        // ignore weak rssi signals from poor lbeacons
        if(rssi_array[k] < rssi_threashold_for_summarize_location_pin)
            continue;

        prev_index = 
            (k - 1 + number_of_rssi_signals_under_tracked) % 
            number_of_rssi_signals_under_tracked;

        // ignore abnormal signal
        if(rssi_array[prev_index] != 0 && 
           abs(rssi_array[k] - rssi_array[prev_index]) > 
           unreasonable_rssi_change){
                        
            continue;
        }
                            
        sum_rssi += rssi_array[k];
        valid_rssi_count++; 
    }
    if(valid_rssi_count == 0 )
        return ret_avg_rssi;

    ret_avg_rssi = sum_rssi / valid_rssi_count;
    return ret_avg_rssi;
}

void hashtable_summarize_location_information(
    HashTable * h_table,
    const int rssi_threashold_for_summarize_location_pin,
    const int number_of_rssi_signals_under_tracked,
    const int unreasonable_rssi_change,
    const int rssi_weight_multiplier,
    const int rssi_difference_of_location_accuracy_tolerance,
    const int drift_distance) {

    int size = h_table->size;
    int table_count = h_table->count;
    int count = 0;
    HNode ** table = h_table->table;
    int i = 0;
    int j = 0;
    int k = 0;
    int prev_index = 0;
    int m = 0;
    int avg_rssi;
    float weight_x;
    float weight_y;
    int weight_count;
    int weight_count_for_specific_uuid;
    float summary_coordinateX_this_turn;
    float summary_coordinateY_this_turn;
    hash_table_row* table_row;  
    pthread_mutex_t * ht_mutex = h_table->ht_mutex; 
    int current_time = get_system_time();

    int summary_index = -1;
    int summary_avg_rssi = INITIAL_AVERAGE_RSSI;
    char summary_uuid[LENGTH_OF_UUID];
    char summary_final_timestamp[LENGTH_OF_EPOCH_TIME];

    int strongest_avg_rssi = INITIAL_AVERAGE_RSSI;
    char strongest_uuid[LENGTH_OF_UUID];
    char strongest_initial_timestamp[LENGTH_OF_EPOCH_TIME];
    char strongest_final_timestamp[LENGTH_OF_EPOCH_TIME];
    HNode * curr = NULL;
    HNode * node_to_release = NULL;
    HNode * prev_node = NULL;

    
    for (i = 0; i < size; i++) {

        curr = table[i];
        prev_node = NULL;
      
        while(curr != NULL){

        if(curr != NULL){

            count++;
        
            //reset the summary data
            summary_index = -1;
            summary_avg_rssi = INITIAL_AVERAGE_RSSI;
            memset(summary_uuid, 0, sizeof(summary_uuid));
            memset(summary_final_timestamp, 0, sizeof(summary_final_timestamp));
            strongest_avg_rssi = INITIAL_AVERAGE_RSSI;
            memset(strongest_uuid, 0, sizeof(strongest_uuid));
            memset(strongest_initial_timestamp, 0, sizeof(strongest_initial_timestamp));
            memset(strongest_final_timestamp, 0, sizeof(strongest_final_timestamp));
          
            table_row = curr -> value; 

            pthread_mutex_lock(&table_row->node_lock);

            //release the old HNode from hashtable to have more space
            if(table_row -> last_reported_timestamp < 
               current_time - 
               TOLERANT_NOT_SCANNING_TIME_IN_SEC ){
               
                // lock hashtable
                pthread_mutex_lock(ht_mutex);

                // re-link the HNode to remove HNode fro hashtable
                if(prev_node == NULL){
                    table[i] = curr -> next;
                }else{
                    prev_node -> next = curr -> next;
                }
                node_to_release = curr;
                curr = curr -> next;
                node_to_release -> next = NULL;

                h_table -> count = h_table->count - 1;
                
                // unlock hashtable
                pthread_mutex_unlock(ht_mutex);

                // destroy key part and release from mempool
                h_table->deleteKey(node_to_release->key);
                mp_free(&mac_address_mempool, 
                        node_to_release->key);
                
                // destroy value part and release from mempool 
                h_table->deleteValue(node_to_release->value);
                mp_free(&hash_table_value_mempool, 
                        node_to_release -> value);

                // destroy HNode part and release from mempool
                mp_free(&hash_table_node_mempool, 
                        node_to_release);
  
                continue;
            }
           
            //calculate the average rssi signal of current summary lbeacon uuid
            for(m = 0; m < table_row -> number_uuid_size; m++){

                if(table_row -> uuid_record_table_array[m].is_in_use &&
                   strcmp(table_row -> uuid_record_table_array[m].uuid,
                          table_row -> summary_uuid) == 0){

                    // ensure current summary lbeacon uuid is still scanning this
                    // object.
                    if(table_row -> uuid_record_table_array[m].
                       last_reported_timestamp < 
                       current_time - number_of_rssi_signals_under_tracked){

                        table_row -> uuid_record_table_array[m].
                            is_in_use = false;
                       
                        break;
                    }

                    // calculate the average rssi
                    avg_rssi = get_average_rssi(
                        table_row->uuid_record_table_array[m].rssi_array,
                        rssi_threashold_for_summarize_location_pin,
                        number_of_rssi_signals_under_tracked,
                        unreasonable_rssi_change);

                    if(avg_rssi != 0){
                        summary_index = m;
                        summary_avg_rssi = avg_rssi;
                        strcpy(summary_uuid, 
                               table_row->uuid_record_table_array[m].uuid);
                        strcpy(summary_final_timestamp,
                               table_row->uuid_record_table_array[m].final_timestamp);  
                    }

                    break;
                }
            }

            // calculate average rssi of all lbeacons and choose the strongest 
            // lbeacon uuid
            weight_x = 0;
            weight_y = 0;
            weight_count = 0;
           
            for(j = 0 ; j < table_row -> number_uuid_size ; j++){
                // ignore not used element 
                if(!table_row->uuid_record_table_array[j].is_in_use) {
                    continue;   
                }

                
                // ignore and delete old lbeacon uuid which has not scanned
                // this object for long time.
                if(table_row -> uuid_record_table_array[j].
                   last_reported_timestamp < 
                   current_time - number_of_rssi_signals_under_tracked){

                    table_row -> uuid_record_table_array[j].
                        is_in_use = false;
                    continue;
                }
                
                // calculate the average rssi
                weight_count_for_specific_uuid = 0;

                // calculate the average rssi
                avg_rssi = get_average_rssi(
                    table_row->uuid_record_table_array[j].rssi_array,
                    rssi_threashold_for_summarize_location_pin,
                    number_of_rssi_signals_under_tracked,
                    unreasonable_rssi_change);

                // ignore this lbeacon uuid if no signal data is used.
                if(avg_rssi == 0){
                    continue;
                }
 
                if(j != summary_index && avg_rssi > strongest_avg_rssi){

                    strongest_avg_rssi = (int) avg_rssi;

                    strcpy(strongest_uuid, 
                           table_row->uuid_record_table_array[j].uuid);
                    strcpy(strongest_initial_timestamp,
                           table_row->uuid_record_table_array[j].initial_timestamp);
                    strcpy(strongest_final_timestamp,
                           table_row->uuid_record_table_array[j].final_timestamp);          
                }

                weight_count_for_specific_uuid = 
                    get_rssi_weight(avg_rssi, rssi_weight_multiplier);

                weight_count += weight_count_for_specific_uuid;

                weight_x += 
                    table_row -> uuid_record_table_array[j].coordinateX * 
                    weight_count_for_specific_uuid;
                weight_y += table_row->uuid_record_table_array[j].coordinateY * 
                    weight_count_for_specific_uuid;               
            }
            
            // update the position of location pin
            if(weight_count > 0){

                summary_coordinateX_this_turn = weight_x/(float)weight_count;
                summary_coordinateY_this_turn = weight_y/(float)weight_count;

                // avoid moving location pins when the objects are not really moved.
                if(abs(summary_coordinateX_this_turn - 
                       table_row->summary_coordinateX) > drift_distance || 
                   abs(summary_coordinateY_this_turn - 
                       table_row->summary_coordinateY) > drift_distance){
                    //coordinateX
                    table_row->summary_coordinateX = 
                        summary_coordinateX_this_turn;
                    //coordinateY
                    table_row->summary_coordinateY = 
                        summary_coordinateY_this_turn;
                }  

                // update the closest lbeacon
                if(summary_index == -1 || 
                   (strongest_avg_rssi - summary_avg_rssi > 
                    rssi_difference_of_location_accuracy_tolerance) ){

                    table_row->average_rssi = strongest_avg_rssi;
                    strcpy(table_row->summary_uuid,
                           strongest_uuid);
                   // MUST use final_timestamp but not initiali_timesatmp to have 
                   // correct lasting time under this newly closest lbeacon uuid.
                   strcpy(table_row->initial_timestamp,
                          strongest_final_timestamp);  
                   strcpy(table_row->final_timestamp,
                           strongest_final_timestamp);
               
                }else{
            
                    table_row->average_rssi = summary_avg_rssi;
                    strcpy(table_row->summary_uuid,
                           summary_uuid);
                    strcpy(table_row->final_timestamp,
                            summary_final_timestamp); 
                }

                table_row->last_reported_timestamp = get_system_time();
            }  

            pthread_mutex_unlock(&table_row->node_lock);
            } // if
        
            curr -> value = table_row;
            prev_node = curr;
            curr = curr->next;
        } // while
     
    } // for-loop
    
}

void hashtable_traverse_areas_to_upload_latest_location(
    DBConnectionListHead *db_connection_list_head,
    const char *server_installation_path,
    AreaSet *area_set,
    const int rssi_threashold_for_summarize_location_pin,
    const int number_of_rssi_signals_under_tracked,
    const int unreasonable_rssi_change,
    const int rssi_weight_multiplier,
    const int rssi_difference_of_location_accuracy_tolerance,
    const int drift_distance){

    int start_index = area_set -> start_area_index;
    int number_areas = area_set -> number_areas;
    
    while(number_areas--){

        if(start_index >= area_table_max_size)
            continue;

        if(area_table[start_index].area_id == 0)
            continue;

        zlog_debug(category_debug,"area table id %d",
                   area_table[start_index].area_id);

        hashtable_summarize_location_information(
            area_table[start_index].area_hash_ptr, 
            rssi_threashold_for_summarize_location_pin,
            number_of_rssi_signals_under_tracked,
            unreasonable_rssi_change,
            rssi_weight_multiplier,
            rssi_difference_of_location_accuracy_tolerance,
            drift_distance);

        hashtable_upload_location_to_database(
            area_table[start_index].area_hash_ptr,
            area_table[start_index].area_id,
            db_connection_list_head,
            server_installation_path,
            LATEST_LOCATION_INFO,
            number_of_rssi_signals_under_tracked);  

        start_index++;
    }
}

void hashtable_traverse_areas_to_upload_history_data(
    DBConnectionListHead *db_connection_list_head,
    const char *server_installation_path,
    AreaSet *area_set,
    const int number_of_rssi_signals_under_tracked){
    
    int start_index = area_set -> start_area_index;
    int number_areas = area_set -> number_areas;

    while(number_areas--){

        if(start_index >= area_table_max_size)
            continue;

        if(area_table[start_index].area_id == 0) 
            continue;

        zlog_debug(category_debug,
                   "hashtable_traverse_all_areas_to_upload_history_data: " \
                   "area table id %d",
                   area_table[start_index].area_id);    

        hashtable_upload_location_to_database(
            area_table[start_index].area_hash_ptr,
            area_table[start_index].area_id,
            db_connection_list_head,
            server_installation_path,
            LOCATION_FOR_HISTORY,
            number_of_rssi_signals_under_tracked);      

        start_index++;
    }
}
    
void hashtable_upload_location_to_database(
    HashTable * h_table,
    int area_id,
    DBConnectionListHead *db_connection_list_head,
    const char *server_installation_path,
    const LocationInfoType location_type,
    const int number_of_rssi_signals_under_tracked) {

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
    char buf_last_reported_time[LENGTH_OF_TIME_FORMAT];
    int clock_time_now;          
    hash_table_row* table_row;   

    int current_time = get_system_time();
    

    if(LOCATION_FOR_HISTORY == location_type){

        sprintf(location_filename, "%s%s_%d", 
                server_installation_path,
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


            if(current_time - table_row->last_reported_timestamp < 
               number_of_rssi_signals_under_tracked && 
               table_row->average_rssi != 0){  
            /*
                zlog_debug(category_debug,"summary:%s %s %s %s %d %d %s\n",
                           table_row->summary_uuid,
                           table_row->battery,
                           table_row->initial_timestamp,
                           table_row->final_timestamp,
                           table_row->last_reported_timestamp, 
                           table_row->average_rssi,
                           table_row->panic_button);
              */                  
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

                    rawtime = table_row ->last_reported_timestamp;
                    ts = *gmtime(&rawtime);
                    strftime(buf_last_reported_time, sizeof(buf_last_reported_time), 
                             "%Y-%m-%d %H:%M:%S", &ts);
                    
                    fprintf(file, "%s,%d,%s,%s,%s,%s,%d,%d,%s,%d\n",
                            table_row->summary_uuid,
                            table_row->average_rssi,
                            table_row->battery,
                            buf_initial_time,
                            buf_final_time,
                            buf_last_reported_time,
                            (int)table_row->summary_coordinateX,
                            (int)table_row->summary_coordinateY,
                            curr->key,
                            area_id);
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
                                       filename,
                                       number_of_rssi_signals_under_tracked);
    }   
}
