/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     sqlWrapper.c

  File Description:

     This file provides APIs for interacting with postgreSQL. This file contains
     programs to connect and disconnect databases, insert, query, update and
     delete data in the database and some APIs for the BeDIS system use
     including updating device status and object tracking data maintainance and
     updates.

  Version:

     1.0, 20191002

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

     Chun-Yu Lai   , chunyu1202@gmail.com
 */

#include "SqlWrapper.h"

static ErrorCode SQL_execute(PGconn *db_conn, char *sql_statement){

    PGresult *res;

    zlog_info(category_debug, "SQL command = [%s]", sql_statement);

    res = PQexec(db_conn, sql_statement);

    if(PQresultStatus(res) != PGRES_COMMAND_OK){

        zlog_error(category_debug, 
                   "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(db_conn));

        PQclear(res);
        return E_SQL_EXECUTE;
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_begin_transaction(PGconn* db_conn){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "BEGIN TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db_conn, sql);

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_commit_transaction(PGconn *db_conn){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "END TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db_conn, sql);

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_rollback_transaction(PGconn *db_conn){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "ROLLBACK;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db_conn, sql);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_create_database_connection_pool(
    char *conninfo, 
    DBConnectionListHead * db_connection_list_head,
    int max_connection){

    int i;
    int retry_times = MEMORY_ALLOCATE_RETRIES;
    DBConnectionNode *db_connection;

    pthread_mutex_lock(&db_connection_list_head->list_lock);

    for(i = 0; i< max_connection; i++){
    
        while(retry_times --){
            db_connection = malloc(sizeof(DBConnectionNode));
            if(NULL != db_connection)
                break;
        }
        if(NULL == db_connection){
        
            zlog_error(category_debug, 
                       "SQL_create_database_connection_pool malloc failed");

            pthread_mutex_unlock(&db_connection_list_head->list_lock);
            return E_MALLOC;
        }
        memset(db_connection, 0, sizeof(DBConnectionNode));

        init_entry(&db_connection->list_entry);

        db_connection->serial_id = i;
        db_connection->is_used = 0;
        db_connection->db = (PGconn*) PQconnectdb(conninfo);

        if(PQstatus(db_connection->db) != CONNECTION_OK){

            zlog_error(category_debug,
                       "Connect to database failed: %s",
                       PQerrorMessage(db_connection->db));

            pthread_mutex_unlock(&db_connection_list_head->list_lock);
            return E_SQL_OPEN_DATABASE;
        }

        insert_list_tail(&db_connection->list_entry, 
                         &db_connection_list_head->list_head);
    }

    pthread_mutex_unlock(&db_connection_list_head->list_lock);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_destroy_database_connection_pool(
    DBConnectionListHead * db_connection_list_head){

    List_Entry *current_list_entry = NULL;
    List_Entry *next_list_entry = NULL;
    DBConnectionNode *current_list_ptr = NULL;
    PGconn * conn = NULL;

    pthread_mutex_lock(&db_connection_list_head->list_lock);

    list_for_each_safe(current_list_entry,
                       next_list_entry, 
                       &db_connection_list_head->list_head){

        current_list_ptr = ListEntry(current_list_entry,
                                     DBConnectionNode,
                                     list_entry);

        conn = (PGconn*) current_list_ptr->db;
        PQfinish(conn);

        remove_list_node(current_list_entry);

        free(current_list_ptr);
    }

    pthread_mutex_unlock(&db_connection_list_head->list_lock);

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_get_database_connection(
    DBConnectionListHead *db_connection_list_head,
    void **db,
    int *serial_id){

    List_Entry *current_list_entry = NULL;
    DBConnectionNode * current_list_ptr = NULL;
    int retry_times = SQL_GET_AVAILABLE_CONNECTION_RETRIES;

    while(retry_times --){

        pthread_mutex_lock(&db_connection_list_head->list_lock);

        list_for_each(current_list_entry,
                      &db_connection_list_head->list_head){
            
            current_list_ptr = ListEntry(current_list_entry,
                                         DBConnectionNode,
                                         list_entry);
       
            if(current_list_ptr->is_used == 0){

               *db = (PGconn*) current_list_ptr->db;
               *serial_id = current_list_ptr->serial_id;
               current_list_ptr->is_used = 1;

               pthread_mutex_unlock(&db_connection_list_head->list_lock);
               return WORK_SUCCESSFULLY;
           } 
        }
        pthread_mutex_unlock(&db_connection_list_head->list_lock);

    }

    return E_SQL_OPEN_DATABASE;
}

static ErrorCode SQL_release_database_connection(
    DBConnectionListHead *db_connection_list_head,
    int serial_id){

    List_Entry *current_list_entry = NULL;
    DBConnectionNode *current_list_ptr = NULL;

    pthread_mutex_lock(&db_connection_list_head->list_lock);

    list_for_each(current_list_entry,
                  &db_connection_list_head->list_head){
        current_list_ptr = ListEntry(current_list_entry,
                                     DBConnectionNode,
                                     list_entry);
        if(current_list_ptr->serial_id == serial_id){
            current_list_ptr->is_used = 0;

            pthread_mutex_unlock(&db_connection_list_head->list_lock);
            return WORK_SUCCESSFULLY;
        }    
    }

    pthread_mutex_unlock(&db_connection_list_head->list_lock);

    return E_SQL_OPEN_DATABASE;
}

ErrorCode SQL_vacuum_database(
    DBConnectionListHead *db_connection_list_head){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *table_name[] = {"location_history_table",
                          "lbeacon_table",
                          "gateway_table",
                          "object_table",
                          "notification_table"};

    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "VACUUM %s;";
    int idx = 0;

    for(idx = 0; idx< sizeof(table_name)/sizeof(table_name[0]) ; idx++){

        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template, table_name[idx]);

        /* Execute SQL statement */
        if(WORK_SUCCESSFULLY != 
           SQL_get_database_connection(db_connection_list_head, 
                                       &db_conn, 
                                       &db_serial_id)){
            zlog_error(category_debug,
                       "cannot operate database");

            continue;
        }

        ret_val = SQL_execute(db_conn, sql);

        if(WORK_SUCCESSFULLY != ret_val){

            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);

            zlog_error(category_debug,
                       "cannot operate database");

            return E_SQL_EXECUTE;
        }

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

    }

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_delete_old_data(
    DBConnectionListHead *db_connection_list_head,                              
    int retention_hours){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *table_name[] = {"notification_table"};
    char *sql_template = "DELETE FROM %s WHERE " \
                         "violation_timestamp < " \
                         "NOW() - INTERVAL \'%d HOURS\';";
    int idx = 0;
    char *tsdb_table_name[] = {"location_history_table"};
    char *sql_tsdb_template = "SELECT drop_chunks(interval \'%d HOURS\', " \
                              "\'%s\');";
    PGresult *res;


    for(idx = 0; idx< sizeof(table_name)/sizeof(table_name[0]); idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_template, table_name[idx], retention_hours);

        if(WORK_SUCCESSFULLY != 
           SQL_get_database_connection(db_connection_list_head, 
                                       &db_conn, 
                                       &db_serial_id)){
            zlog_error(category_debug,
                       "cannot operate database\n");

            continue;
        }

        ret_val = SQL_execute(db_conn, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            
            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);

            zlog_error(category_debug,
                       "cannot operate database\n");

            return E_SQL_EXECUTE;
        }
        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);
    }

    for(idx = 0; 
        idx< sizeof(tsdb_table_name)/sizeof(tsdb_table_name[0]); 
        idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_tsdb_template, retention_hours, 
                tsdb_table_name[idx]);

        /* Execute SQL statement */
        zlog_info(category_debug, "SQL command = [%s]", sql);

        if(WORK_SUCCESSFULLY != 
           SQL_get_database_connection(db_connection_list_head, 
                                       &db_conn, 
                                       &db_serial_id)){
            zlog_error(category_debug,
                       "cannot operate database\n");

            continue;
        }
        res = PQexec(db_conn, sql);
        if(PQresultStatus(res) != PGRES_TUPLES_OK){

            PQclear(res);
            zlog_info(category_debug, "SQL_execute failed: %s", 
                      PQerrorMessage(db_conn));

            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);

            return E_SQL_EXECUTE;

        }
        PQclear(res);
        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_gateway_registration_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char *numbers_str = NULL;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_template = 
        "INSERT INTO gateway_table " \
        "(ip_address, " \
        "health_status, " \
        "registered_timestamp, " \
        "last_report_timestamp, " \
        "api_version) " \
        "VALUES " \
        "(%s, \'%d\', NOW(), NOW(), %s)" \
        "ON CONFLICT (ip_address) " \
        "DO UPDATE SET " \
        "health_status = \'%d\', " \
        "last_report_timestamp = NOW(), " \
        "api_version = %s;";

    HealthStatus health_status = S_NORMAL_STATUS;
    char *ip_address = NULL;
    char *status = NULL;
    char *api_version = NULL;
    char *pqescape_ip_address = NULL;
    char *pqescape_api_version = NULL;
   

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    numbers_str = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    if(numbers_str == NULL){
        return E_API_PROTOCOL_FORMAT;
    }
    numbers = atoi(numbers_str);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    while( numbers-- ){
        
        ip_address = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        status = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        api_version = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        /* Create SQL statement */
        pqescape_ip_address =
            PQescapeLiteral(db_conn, ip_address, strlen(ip_address));

        pqescape_api_version =
            PQescapeLiteral(db_conn, api_version, strlen(api_version));

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_template,
                pqescape_ip_address,
                health_status,
                pqescape_api_version,
                health_status,
                pqescape_api_version);
       
        PQfreemem(pqescape_ip_address);
        PQfreemem(pqescape_api_version);

        /* Execute SQL statement */
        ret_val = SQL_execute(db_conn, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            
            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);

            return E_SQL_EXECUTE;
        }
    }

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_lbeacon_registration_status_less_ver22(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char *numbers_str = NULL;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = 
        "INSERT INTO lbeacon_table " \
        "(uuid, " \
        "ip_address, " \
        "health_status, " \
        "gateway_ip_address, " \
        "registered_timestamp, " \
        "last_report_timestamp, " \
        "coordinate_x, " \
        "coordinate_y) " \
        "VALUES " \
        "(%s, %s, \'%d\', %s, " \
        "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
        "NOW(), " \
        "%d, %d) " \
        "ON CONFLICT (uuid) " \
        "DO UPDATE SET " \
        "ip_address = %s, " \
        "health_status = \'%d\', " \
        "gateway_ip_address = %s, " \
        "last_report_timestamp = NOW(), " \
        "coordinate_x = %d, " \
        "coordinate_y = %d;";

    HealthStatus health_status = S_NORMAL_STATUS;
    char *uuid = NULL;
    char *lbeacon_ip = NULL;
    char *not_used_gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;
    char *pqescape_uuid = NULL;
    char *pqescape_lbeacon_ip = NULL;
    char *pqescape_gateway_ip = NULL;
    char *pqescape_registered_timestamp_GMT = NULL;
    char str_uuid[LENGTH_OF_UUID];
    char coordinate_x[LENGTH_OF_UUID];
    char coordinate_y[LENGTH_OF_UUID];
    int int_coordinate_x = 0;
    int int_coordinate_y = 0;
    int current_time = get_system_time();
    
	
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    numbers_str = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    if(numbers_str == NULL){
        return E_API_PROTOCOL_FORMAT;
    }
    numbers = atoi(numbers_str);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    not_used_gateway_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    while( numbers-- ){
        uuid = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        memset(str_uuid, 0, sizeof(str_uuid));
        strcpy(str_uuid, uuid);

        memset(coordinate_x, 0, sizeof(coordinate_x));
        memset(coordinate_y, 0, sizeof(coordinate_y));
        
        strncpy(coordinate_x, 
                &str_uuid[INDEX_OF_COORDINATE_X_IN_UUID], 
                LENGTH_OF_COORDINATE_IN_UUID);
        strncpy(coordinate_y, 
                &str_uuid[INDEX_OF_COORDINATE_Y_IN_UUID], 
                LENGTH_OF_COORDINATE_IN_UUID);

        int_coordinate_x = atoi(coordinate_x);
        int_coordinate_y = atoi(coordinate_y);

        registered_timestamp_GMT = 
            strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
 
        lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));

        pqescape_uuid = 
            PQescapeLiteral(db_conn, uuid, strlen(uuid));

        pqescape_lbeacon_ip =
            PQescapeLiteral(db_conn, lbeacon_ip, strlen(lbeacon_ip));

        pqescape_gateway_ip =
            PQescapeLiteral(db_conn, gateway_ip_address, 
                            strlen(gateway_ip_address));
        pqescape_registered_timestamp_GMT =
            PQescapeLiteral(db_conn, registered_timestamp_GMT,
                            strlen(registered_timestamp_GMT));

        sprintf(sql, sql_template,
                pqescape_uuid,
                pqescape_lbeacon_ip,
                health_status,
                pqescape_gateway_ip,
                pqescape_registered_timestamp_GMT,
                int_coordinate_x,
                int_coordinate_y,
                pqescape_lbeacon_ip,
                health_status,
                pqescape_gateway_ip,
                int_coordinate_x,
                int_coordinate_y);

        PQfreemem(pqescape_uuid);
        PQfreemem(pqescape_lbeacon_ip);
        PQfreemem(pqescape_gateway_ip);
        PQfreemem(pqescape_registered_timestamp_GMT);

        /* Execute SQL statement */
        ret_val = SQL_execute(db_conn, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);
            return E_SQL_EXECUTE;
        }

    }

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_lbeacon_registration_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char *numbers_str = NULL;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = 
        "INSERT INTO lbeacon_table " \
        "(uuid, " \
        "ip_address, " \
        "health_status, " \
        "gateway_ip_address, " \
        "registered_timestamp, " \
        "last_report_timestamp, " \
        "api_version, " \
        "coordinate_x, " \
        "coordinate_y) " \
        "VALUES " \
        "(%s, %s, \'%d\', %s, " \
        "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
        "NOW(), " \
        "%s, " \
        "%d, %d) " \
        "ON CONFLICT (uuid) " \
        "DO UPDATE SET " \
        "ip_address = %s, " \
        "health_status = \'%d\', " \
        "gateway_ip_address = %s, " \
        "last_report_timestamp = NOW(), " \
        "api_version = %s, " \
        "coordinate_x = %d, " \
        "coordinate_y = %d;";

    HealthStatus health_status = S_NORMAL_STATUS;
    char *uuid = NULL;
    char *lbeacon_ip = NULL;
    char *not_used_gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;
    char *api_version = NULL;
    char *pqescape_uuid = NULL;
    char *pqescape_lbeacon_ip = NULL;
    char *pqescape_gateway_ip = NULL;
    char *pqescape_registered_timestamp_GMT = NULL;
    char *pqescape_api_version = NULL;
    char str_uuid[LENGTH_OF_UUID];
    char coordinate_x[LENGTH_OF_UUID];
    char coordinate_y[LENGTH_OF_UUID];
    int int_coordinate_x = 0;
    int int_coordinate_y = 0;

    
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

   
    numbers_str = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    if(numbers_str == NULL){
        return E_API_PROTOCOL_FORMAT;
    }
    numbers = atoi(numbers_str);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    not_used_gateway_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    while( numbers-- ){
        uuid = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        memset(str_uuid, 0, sizeof(str_uuid));
        strcpy(str_uuid, uuid);

        memset(coordinate_x, 0, sizeof(coordinate_x));
        memset(coordinate_y, 0, sizeof(coordinate_y));
        
        strncpy(coordinate_x, 
                &str_uuid[INDEX_OF_COORDINATE_X_IN_UUID], 
                LENGTH_OF_COORDINATE_IN_UUID);
        strncpy(coordinate_y, 
                &str_uuid[INDEX_OF_COORDINATE_Y_IN_UUID], 
                LENGTH_OF_COORDINATE_IN_UUID);

        int_coordinate_x = atoi(coordinate_x);
        int_coordinate_y = atoi(coordinate_y);

        registered_timestamp_GMT = 
            strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        

        lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        api_version = 
            strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));

        pqescape_uuid = 
            PQescapeLiteral(db_conn, uuid, strlen(uuid));

        pqescape_lbeacon_ip =
            PQescapeLiteral(db_conn, lbeacon_ip, strlen(lbeacon_ip));

        pqescape_registered_timestamp_GMT =
            PQescapeLiteral(db_conn, registered_timestamp_GMT,
                            strlen(registered_timestamp_GMT));
        pqescape_gateway_ip =
            PQescapeLiteral(db_conn, gateway_ip_address, 
                            strlen(gateway_ip_address));
 
        pqescape_api_version =
            PQescapeLiteral(db_conn, api_version,
                            strlen(api_version));

        sprintf(sql, sql_template,
                pqescape_uuid,
                pqescape_lbeacon_ip,
                health_status,
                pqescape_gateway_ip,
                pqescape_registered_timestamp_GMT,
                pqescape_api_version,
                int_coordinate_x,
                int_coordinate_y,
                pqescape_lbeacon_ip,
                health_status,
                pqescape_gateway_ip,
                pqescape_api_version,
                int_coordinate_x,
                int_coordinate_y);

        PQfreemem(pqescape_uuid);
        PQfreemem(pqescape_lbeacon_ip);
        PQfreemem(pqescape_gateway_ip);
        PQfreemem(pqescape_registered_timestamp_GMT);
        PQfreemem(pqescape_api_version);

        /* Execute SQL statement */
        ret_val = SQL_execute(db_conn, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);
            return E_SQL_EXECUTE;
        }

    }

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_gateway_health_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "UPDATE gateway_table " \
                         "SET health_status = %s, " \
                         "last_report_timestamp = NOW() " \
                         "WHERE ip_address = %s ;" ;
    char *not_used_ip_address = NULL;
    char *health_status = NULL;
    char *pqescape_ip_address = NULL;
    char *pqescape_health_status = NULL;


    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    not_used_ip_address = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    health_status = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    /* Create SQL statement */
    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    pqescape_ip_address =
        PQescapeLiteral(db_conn, gateway_ip_address, strlen(gateway_ip_address));
    pqescape_health_status =
        PQescapeLiteral(db_conn, health_status, strlen(health_status));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template,
            pqescape_health_status,
            pqescape_ip_address);

    PQfreemem(pqescape_ip_address);
    PQfreemem(pqescape_health_status);

    /* Execute SQL statement */
    ret_val = SQL_execute(db_conn, sql);

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    if(WORK_SUCCESSFULLY != ret_val){
    
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_lbeacon_health_status(
    DBConnectionListHead *db_connection_list_head,
    char *buf,
    size_t buf_len,
    char *gateway_ip_address){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "UPDATE lbeacon_table " \
                         "SET health_status = %s, " \
                         "last_report_timestamp = NOW(), " \
						 "gateway_ip_address = %s, " \
                         "server_time_offset = %d " \
                         "WHERE uuid = %s ;";
    char *lbeacon_uuid = NULL;
    char *lbeacon_timestamp = NULL;
    char *lbeacon_ip = NULL;
    char *health_status = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_health_status = NULL;
    char *pqescape_gateway_ip = NULL;
    int current_time = get_system_time();
    int lbeacon_timestamp_value = 0;
   
 
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);	
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_timestamp_value = atoi(lbeacon_timestamp);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    health_status = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);


    /* Create SQL statement */
    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }
    pqescape_lbeacon_uuid = 
        PQescapeLiteral(db_conn, lbeacon_uuid, strlen(lbeacon_uuid));
    pqescape_health_status =
        PQescapeLiteral(db_conn, health_status, strlen(health_status));
    pqescape_gateway_ip = 
        PQescapeLiteral(db_conn, gateway_ip_address, strlen(gateway_ip_address));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template,
            pqescape_health_status,
		    pqescape_gateway_ip,
            current_time - lbeacon_timestamp_value,
            pqescape_lbeacon_uuid);

    PQfreemem(pqescape_lbeacon_uuid);
    PQfreemem(pqescape_health_status);
    PQfreemem(pqescape_gateway_ip);

    /* Execute SQL statement */
    ret_val = SQL_execute(db_conn, sql);

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    if(WORK_SUCCESSFULLY != ret_val){
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_geofence_violation(
    DBConnectionListHead *db_connection_list_head,
    char *mac_address){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_insert_summary_table = 
        "UPDATE object_summary_table " \
        "SET " \
        "geofence_violation_timestamp = NOW() " \
        "WHERE mac_address = %s";

    char *pqescape_mac_address = NULL;

    memset(sql, 0, sizeof(sql));

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    pqescape_mac_address = 
        PQescapeLiteral(db_conn, mac_address, 
                        strlen(mac_address)); 
   
    sprintf(sql, 
            sql_insert_summary_table, 
            pqescape_mac_address);
    
    ret_val = SQL_execute(db_conn, sql);

    PQfreemem(pqescape_mac_address);
   
    if(WORK_SUCCESSFULLY != ret_val){
        
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        return E_SQL_EXECUTE;
    }     

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);
    
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_location_not_stay_room(
    DBConnectionListHead *db_connection_list_head){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "UPDATE object_summary_table " \
                                "SET " \
                                "location_violation_timestamp = NOW() " \
                                "FROM " \
                                "(SELECT " \
                                "object_summary_table.mac_address, " \
                                "object_summary_table.uuid, " \
                                "monitor_type, " \
                                "lbeacon_table.room, " \
                                "object_table.room " \
                                "FROM "\
                                "object_summary_table " \
                                "INNER JOIN object_table ON " \
                                "object_summary_table.mac_address = " \
                                "object_table.mac_address " \
                                "INNER JOIN lbeacon_table ON " \
                                "object_summary_table.uuid = " \
                                "lbeacon_table.uuid " \
                                "INNER JOIN location_not_stay_room_config ON " \
                                "object_table.area_id = " \
                                "location_not_stay_room_config.area_id " \
                                "WHERE " \
                                "location_not_stay_room_config.is_active = 1 " \
                                "AND monitor_type & %d = %d " \
                                "AND object_table.room IS NOT NULL " \
                                "AND (lbeacon_table.room IS NULL OR lbeacon_table.room <> object_table.room) " \
                                ") location_information " \
                                "WHERE object_summary_table.mac_address = " \
                                "location_information.mac_address;";

    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            MONITOR_LOCATION,
            MONITOR_LOCATION);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    ret_val = SQL_execute(db_conn, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        return E_SQL_EXECUTE;
    }

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);
    
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_location_long_stay_in_danger(
    DBConnectionListHead *db_connection_list_head){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "UPDATE object_summary_table " \
                                "SET " \
                                "location_violation_timestamp = NOW() " \
                                "FROM " \
                                "(SELECT " \
                                "object_summary_table.mac_address, " \
                                "object_summary_table.uuid, " \
                                "monitor_type, " \
                                "danger_area " \
                                "FROM "\
                                "object_summary_table " \
                                "INNER JOIN object_table ON " \
                                "object_summary_table.mac_address = " \
                                "object_table.mac_address " \
                                "INNER JOIN lbeacon_table ON " \
                                "object_summary_table.uuid = " \
                                "lbeacon_table.uuid " \
                                "INNER JOIN location_long_stay_in_danger_config ON " \
                                "object_table.area_id = " \
                                "location_long_stay_in_danger_config.area_id " \
                                "WHERE " \
                                "location_long_stay_in_danger_config.is_active = 1 " \
                                "AND monitor_type & %d = %d " \
                                "AND danger_area = 1 " \
                                "AND EXTRACT(MIN FROM last_seen_timestamp - " \
                                "first_seen_timestamp) > "\
                                "location_long_stay_in_danger_config.stay_duration " \
                                ") location_information " \
                                "WHERE object_summary_table.mac_address = " \
                                "location_information.mac_address;";


    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            MONITOR_LOCATION,
            MONITOR_LOCATION);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    ret_val = SQL_execute(db_conn, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        return E_SQL_EXECUTE;
    }

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_last_movement_status(
    DBConnectionListHead *db_connection_list_head,
    int time_interval_in_min, 
    int each_time_slot_in_min,
    unsigned int rssi_delta){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_select_template = "SELECT " \
                                "object_summary_table.mac_address, " \
                                "object_summary_table.uuid " \
                                "FROM object_summary_table " \
                                "INNER JOIN object_table ON " \
                                "object_summary_table.mac_address = " \
                                "object_table.mac_address " \
                                "INNER JOIN movement_config ON " \
                                "object_table.area_id = " \
                                "movement_config.area_id " \
                                "WHERE " \
                                "movement_config.is_active = 1 AND " \
                                "object_table.monitor_type & %d = %d " \
                                "ORDER BY " \
                                "mac_address ASC";

    const int NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE = 2;
    const int FIELD_INDEX_OF_MAC_ADDRESS = 0;
    const int FIELD_INDEX_OF_UUID = 1;

    PGresult *res = NULL;
    int current_row = 0;
    int total_fields = 0;
    int total_rows = 0;

    char *mac_address = NULL;
    char *lbeacon_uuid = NULL;

    char *sql_select_activity_template = 
        "SELECT time_slot, avg_rssi, diff " \
        "FROM ( " \
        "SELECT time_slot, avg_rssi, avg_rssi - LAG(avg_rssi) " \
        "OVER (ORDER BY time_slot) as diff " \
        "FROM ( " \
        "SELECT TIME_BUCKET('%d minutes', record_timestamp) as time_slot, " \
        "AVG(average_rssi) as avg_rssi " \
        "FROM location_history_table where " \
        "record_timestamp > NOW() - INTERVAL '%d minutes' " \
        "AND uuid = %s " \
        "AND mac_address = %s " \
        "GROUP BY time_slot" \
        ") " \
        "AS temp_time_slot_table )" \
        "AS temp_delta " \
        "WHERE diff > %d or diff < %d " \
        "ORDER BY time_slot DESC;";

    char *pqescape_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;

    PGresult *res_activity = NULL;
    int rows_activity = 0;
   
    char *time_slot_activity = NULL;

    char *sql_update_activity_template = 
        "UPDATE object_summary_table " \
        "SET movement_violation_timestamp = NOW()" \
        "WHERE mac_address = %s";
  
    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template,
            MONITOR_MOVEMENT,
            MONITOR_MOVEMENT);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    res = PQexec(db_conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){

        for(current_row = 0 ; current_row < total_rows ; current_row++){
            mac_address = PQgetvalue(res, 
                                     current_row, 
                                     FIELD_INDEX_OF_MAC_ADDRESS);

            lbeacon_uuid = PQgetvalue(res, 
                                      current_row, 
                                      FIELD_INDEX_OF_UUID);

            if(strlen(lbeacon_uuid) == 0){
                continue;
            }

            pqescape_mac_address = 
                PQescapeLiteral(db_conn, mac_address, strlen(mac_address));
            pqescape_lbeacon_uuid = 
                PQescapeLiteral(db_conn, lbeacon_uuid, strlen(lbeacon_uuid));

            sprintf(sql, sql_select_activity_template, 
                    each_time_slot_in_min,
                    time_interval_in_min,
                    pqescape_lbeacon_uuid,
                    pqescape_mac_address,
                    rssi_delta,
                    0 - rssi_delta);

            res_activity = PQexec(db_conn, sql);

            PQfreemem(pqescape_mac_address);
            PQfreemem(pqescape_lbeacon_uuid);

            if(PQresultStatus(res_activity) != PGRES_TUPLES_OK){
                PQclear(res_activity);
                PQclear(res);
                    
                zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                           res_activity, PQerrorMessage(db_conn));

                SQL_release_database_connection(
                    db_connection_list_head,
                    db_serial_id);

                return E_SQL_EXECUTE;
            }

            rows_activity = PQntuples(res_activity);
         
            if(rows_activity == 0){
                pqescape_mac_address = 
                    PQescapeLiteral(db_conn, mac_address, 
                                    strlen(mac_address));
                
                memset(sql, 0, sizeof(sql));
                    
                sprintf(sql, sql_update_activity_template,
                        pqescape_mac_address);
                            
                ret_val = SQL_execute(db_conn, sql);

                PQfreemem(pqescape_mac_address);
               
                if(WORK_SUCCESSFULLY != ret_val){
                    PQclear(res_activity);   
                    PQclear(res);

                    zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                               ret_val, PQerrorMessage(db_conn));

                    SQL_release_database_connection(
                        db_connection_list_head,
                        db_serial_id);

                    return E_SQL_EXECUTE;
                }     
                PQclear(res_activity);

                continue;
            }
            PQclear(res_activity);
        }
    }

    PQclear(res);
    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_collect_violation_events(
    DBConnectionListHead *db_connection_list_head,
    ObjectMonitorType monitor_type,
    int time_interval_in_sec,
    int granularity_for_continuous_violations_in_sec){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_insert_template = 
        "INSERT INTO " \
        "notification_table( " \
        "monitor_type, " \
        "mac_address, " \
        "uuid, " \
        "violation_timestamp, " \
        "processed) " \
        "SELECT %d, " \
        "mac_address, " \
        "uuid, " \
        "%s, " \
        "0 " \
        "FROM object_summary_table " \
        "WHERE "\
        "%s >= " \
        "NOW() - interval '%d seconds' " \
        "AND NOT EXISTS(" \
        "SELECT * FROM notification_table " \
        "WHERE monitor_type = %d " \
        "AND mac_address = mac_address " \
        "AND uuid = uuid " \
        "AND EXTRACT(EPOCH FROM(%s - " \
        "violation_timestamp)) < %d);";

    char *geofence_violation_timestamp = "geofence_violation_timestamp";
    char *panic_violation_timestamp = "panic_violation_timestamp";
    char *movement_violation_timestamp = "movement_violation_timestamp";
    char *location_violation_timestamp = "location_violation_timestamp";
    char *violation_timestamp_name = NULL;

    switch (monitor_type){
        case MONITOR_GEO_FENCE:
            violation_timestamp_name = geofence_violation_timestamp;
            break;
        case MONITOR_PANIC:
            violation_timestamp_name = panic_violation_timestamp;
            break;
        case MONITOR_MOVEMENT:
            violation_timestamp_name = movement_violation_timestamp;
            break;
        case MONITOR_LOCATION:
            violation_timestamp_name = location_violation_timestamp;
            break;
        default:
            zlog_error(category_debug, "Unknown monitor_type=[%d]", 
                       monitor_type);
            return E_INPUT_PARAMETER;
    }

    memset(sql, 0, sizeof(sql));
    sprintf(sql, 
            sql_insert_template, 
            monitor_type, 
            violation_timestamp_name,
            violation_timestamp_name,
            time_interval_in_sec,
            monitor_type,
            violation_timestamp_name,
            granularity_for_continuous_violations_in_sec);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    ret_val = SQL_execute(db_conn, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        
        zlog_error(category_debug, 
                   "SQL_execute failed [%d]: %s", 
                   ret_val, 
                   PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        return E_SQL_EXECUTE;
    }    
    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_get_and_update_violation_events(
    DBConnectionListHead *db_connection_list_head,
    int server_localtime_against_UTC_in_hour,
    char *buf,
    size_t buf_len){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_select_template = 
        "SELECT " \
        "notification_table.id, " \
        "monitor_type_table.readable_name , " \
        "notification_table.mac_address, " \
        "notification_table.uuid, " \
        "notification_table.violation_timestamp + interval '%d hours' , " \
        "area_table.readable_name , " \
        "object_table.object_type, " \
        "object_table.name, " \
        "object_table.asset_control_number, " \
        "lbeacon_table.description " \
        "FROM " \
        "notification_table " \
        "INNER JOIN object_table " \
        "ON notification_table.mac_address = object_table.mac_address " \
        "INNER JOIN area_table " \
        "ON area_table.id = object_table.area_id " \
        "INNER JOIN lbeacon_table " \
        "ON notification_table.uuid = lbeacon_table.uuid " \
        "INNER JOIN monitor_type_table " \
        "ON notification_table.monitor_type = monitor_type_table.type_id " \
        "WHERE "\
        "processed != 1 " \
        "ORDER BY id ASC;";

    const int NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE = 10;
    const int FIELD_INDEX_OF_ID = 0;
    const int FIELD_INDEX_OF_MONITOR_TYPE = 1;
    const int FIELD_INDEX_OF_MAC_ADDRESS = 2;
    const int FIELD_INDEX_OF_UUID = 3;
    const int FIELD_INDEX_OF_VIOLATION_TIMESTAMP = 4;
    const int FIELD_INDEX_OF_AREA_NAME = 5;
    const int FIELD_INDEX_OF_OBJECT_TYPE = 6;
    const int FIELD_INDEX_OF_OBJECT_NAME = 7;
    const int FIELD_INDEX_OF_OBJECT_IDENTITY = 8;
    const int FIELD_INDEX_OF_LBEACON_DESCRIPTION = 9;

    PGresult *res = NULL;
    int total_fields = 0;
    int total_rows = 0;
    int i;
    char one_record[SQL_TEMP_BUFFER_LENGTH];
    char *sql_update_template = 
        "UPDATE "
        "notification_table " \
        "SET "\
        "processed = 1 " \
        "WHERE id = %d;";

    int id_int;


    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_select_template, server_localtime_against_UTC_in_hour);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot operate database");

        return E_SQL_OPEN_DATABASE;
    }

    res = PQexec(db_conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        return E_SQL_EXECUTE;
    }

    total_rows = PQntuples(res);
    total_fields = PQnfields(res);
    
    sprintf(buf, "%d;", total_rows);

    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){
        for(i = 0 ; i < total_rows ; i++){
            memset(one_record, 0, sizeof(one_record));
            sprintf(one_record, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s;", 
                    PQgetvalue(res, i, FIELD_INDEX_OF_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_MONITOR_TYPE),
                    PQgetvalue(res, i, FIELD_INDEX_OF_MAC_ADDRESS),
                    PQgetvalue(res, i, FIELD_INDEX_OF_UUID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_VIOLATION_TIMESTAMP),
                    PQgetvalue(res, i, FIELD_INDEX_OF_AREA_NAME),
                    PQgetvalue(res, i, FIELD_INDEX_OF_OBJECT_TYPE),
                    PQgetvalue(res, i, FIELD_INDEX_OF_OBJECT_NAME),
                    PQgetvalue(res, i, FIELD_INDEX_OF_OBJECT_IDENTITY),
                    PQgetvalue(res, i, FIELD_INDEX_OF_LBEACON_DESCRIPTION));
            
            if(buf_len > strlen(buf) + strlen(one_record)){
                strcat(buf, one_record);
            
                memset(sql, 0, sizeof(sql));
                if(PQgetvalue(res, i, FIELD_INDEX_OF_ID) == NULL){
                    PQclear(res);

                    SQL_release_database_connection(
                        db_connection_list_head,
                        db_serial_id);

                    return E_API_PROTOCOL_FORMAT;
                }
                sprintf(sql, 
                        sql_update_template, 
                        atoi(PQgetvalue(res, i, FIELD_INDEX_OF_ID)));

                ret_val = SQL_execute(db_conn, sql);        
            }
        }
    }

    PQclear(res);
    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_reload_monitor_config(
    DBConnectionListHead *db_connection_list_head,
    int server_localtime_against_UTC_in_hour)
{
    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *table_name[] = {"geo_fence_config",
                          "location_not_stay_room_config", 
                          "location_long_stay_in_danger_config",
                          "movement_config" };

    char *sql_update_template = "UPDATE %s " \
                                "SET is_active = CASE " \
                                "WHEN " \
                                "(enable = 1 AND " \
                                "start_time < end_time AND " \
                                "CURRENT_TIME + interval '%d hours' >= " \
                                "start_time AND " \
                                "CURRENT_TIME + interval '%d hours' < " \
                                "end_time)" \
                                "OR " \
                                "(enable = 1 AND " \
                                "start_time > end_time AND " \
                                "(" \
                                "(CURRENT_TIME + interval '%d hours' >= " \
                                "start_time AND " \
                                "CURRENT_TIME + INTERVAL '%d hours' <= " \
                                "'23:59:59') " \
                                "OR " \
                                "(CURRENT_TIME + INTERVAL '%d hours' >= " \
                                "'00:00:00' AND " \
                                "CURRENT_TIME + INTERVAL '%d hours' < " \
                                "end_time)" \
                                ")" \
                                ") " \
                                "THEN 1" \
                                "ELSE 0" \
                                "END;";

    PGresult *res = NULL;
    int idx = 0;

    for(idx = 0; idx< sizeof(table_name)/sizeof(table_name[0]); idx++){
        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_update_template, 
                table_name[idx],
                server_localtime_against_UTC_in_hour,
                server_localtime_against_UTC_in_hour,
                server_localtime_against_UTC_in_hour,
                server_localtime_against_UTC_in_hour,
                server_localtime_against_UTC_in_hour,
                server_localtime_against_UTC_in_hour);

        if(WORK_SUCCESSFULLY != 
           SQL_get_database_connection(db_connection_list_head, 
                                       &db_conn, 
                                       &db_serial_id)){
            zlog_error(category_debug,
                       "cannot operate database");

            return E_SQL_OPEN_DATABASE;
        }

        ret_val = SQL_execute(db_conn, sql);

        if(WORK_SUCCESSFULLY != ret_val){
        
            zlog_error(category_debug, 
                       "SQL_execute failed [%d]: %s", 
                       ret_val, 
                       PQerrorMessage(db_conn));
    
            SQL_release_database_connection(
                db_connection_list_head,
                db_serial_id);

            return E_SQL_EXECUTE;
        }     

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_dump_active_geo_fence_settings(
    DBConnectionListHead *db_connection_list_head, 
    char *filename)
{
    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    
    char *sql_select_template = "SELECT " \
                                "area_id, " \
                                "is_global_fence, " \
                                "id, " \
                                "name, " \
                                "perimeters, " \
                                "fences " \
                                "FROM geo_fence_config " \
                                "WHERE " \
                                "is_active = 1;";

    const int NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE = 6;
    const int FIELD_INDEX_OF_AREA_ID = 0;
    const int FIELD_INDEX_OF_IS_GLOBAL_FENCE = 1;
    const int FIELD_INDEX_OF_ID = 2;
    const int FIELD_INDEX_OF_NAME = 3;
    const int FIELD_INDEX_OF_PERIMETRS = 4;
    const int FIELD_INDEX_OF_FENCES = 5;

    FILE *file = NULL;

    PGresult *res = NULL;
    int total_fields = 0;
    int total_rows = 0;
    int i = 0;

    file = fopen(filename, "wt");
    if(file == NULL){
        zlog_error(category_debug, "cannot open filepath %s", filename);
        return E_OPEN_FILE;
    }

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_select_template);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
       zlog_error(category_debug,
                  "cannot open database");

       fclose(file);

       return E_SQL_OPEN_DATABASE;
    }

    res = PQexec(db_conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){

        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head, 
            db_serial_id);

        fclose(file);

        return E_SQL_EXECUTE;
    }

    total_rows = PQntuples(res);
    total_fields = PQnfields(res);
    
    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){
         
        for(i = 0 ; i < total_rows ; i++){
            fprintf(file, "%s;%s;%s;%s;%s;%s;\n", 
                    PQgetvalue(res, i, FIELD_INDEX_OF_AREA_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_IS_GLOBAL_FENCE),
                    PQgetvalue(res, i, FIELD_INDEX_OF_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_NAME),
                    PQgetvalue(res, i, FIELD_INDEX_OF_PERIMETRS),
                    PQgetvalue(res, i, FIELD_INDEX_OF_FENCES));
        }
    }

    PQclear(res);

    SQL_release_database_connection(
        db_connection_list_head, 
        db_serial_id);

    fclose(file);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_dump_mac_address_under_geo_fence_monitor(
    DBConnectionListHead *db_connection_list_head, 
    char *filename){

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    
    char *sql_select_template = "SELECT " \
                                "area_id, " \
                                "mac_address " \
                                "FROM object_table " \
                                "WHERE " \
                                "monitor_type & %d = %d " \
                                "ORDER BY area_id ASC;";

    const int NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE = 2;
    const int FIELD_INDEX_OF_AREA_ID = 0;
    const int FIELD_INDEX_OF_MAC_ADDRESS = 1;
    
    FILE *file = NULL;

    PGresult *res = NULL;
    int total_fields = 0;
    int total_rows = 0;
    int i = 0;

    file = fopen(filename, "wt");
    if(file == NULL){
        zlog_error(category_debug, "cannot open filepath %s", filename);
        return E_OPEN_FILE;
    }

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_select_template, 
            MONITOR_GEO_FENCE, 
            MONITOR_GEO_FENCE);

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
       zlog_error(category_debug,
                  "cannot open database\n");

       fclose(file);

       return E_SQL_OPEN_DATABASE;
    }

    res = PQexec(db_conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){

        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(db_conn));

        SQL_release_database_connection(
            db_connection_list_head,
            db_serial_id);

        fclose(file);

        return E_SQL_EXECUTE;
    }

    total_rows = PQntuples(res);
    total_fields = PQnfields(res);
    
    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){
         
        for(i = 0 ; i < total_rows ; i++){

            fprintf(file, "%s;%s;\n", 
                    PQgetvalue(res, i, FIELD_INDEX_OF_AREA_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_MAC_ADDRESS));
        }
    }

    PQclear(res);

    SQL_release_database_connection(
        db_connection_list_head,
        db_serial_id);

    fclose(file);

    return WORK_SUCCESSFULLY;

}

ErrorCode SQL_upload_hashtable_summarize(
    DBConnectionListHead *db_connection_list_head,
    char* filename){
		
    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];	
    char *sql_create_temp_table=
        "CREATE TEMP TABLE updates_table " \
        "( " \
        "uuid uuid , " \
        "rssi integer , " \
        "first_seen_timestamp timestamp with time zone , " \
        "last_seen_timestamp timestamp with time zone , " \
        "last_reported_timestamp timestamp with time zone , " \
        "battery_voltage smallint , " \
        "base_x bigint , " \
        "base_y bigint , " \
        "mac_address macaddr not null primary key " \
        "); ";
	 
    char* sql_bulk_insert=
        "COPY updates_table " \
        "( " \
        "uuid , " \
        "rssi , " \
        "battery_voltage , " \
        "first_seen_timestamp , " \
        "last_seen_timestamp , " \
        "last_reported_timestamp , " \
        "base_x , " \
        "base_y , " \
        "mac_address)" \
        "FROM " \
        "\'%s\' " \
        "DELIMITER \',\' CSV; ";
			 
    char* sql_update=
        "UPDATE object_summary_table s " \
        "SET ( " \
        "uuid , " \
        "rssi , " \
        "battery_voltage , " \
        "first_seen_timestamp , " \
        "last_seen_timestamp , " \
        "last_reported_timestamp , " \
        "base_x , " \
        "base_y  " \
        ") = (" \
        "t.uuid , " \
        "t.rssi , " \
        "t.battery_voltage , " \
        "t.first_seen_timestamp , " \
        "t.last_seen_timestamp , " \
        "t.last_reported_timestamp , " \
        "t.base_x , " \
        "t.base_y  ) " \
        "FROM updates_table t " \
        "WHERE s.mac_address = t.mac_address; ";
			 
    char* drop_temp=
        "DROP TABLE updates_table; ";
	
	
    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot open database\n");

        return E_SQL_OPEN_DATABASE;
    }
	
    // begin transaction
    SQL_begin_transaction(db_conn);
	
    // create temp table updates_table
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_create_temp_table);
    ret_val = SQL_execute(db_conn, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        remove(filename);
        SQL_rollback_transaction(db_conn);

        SQL_release_database_connection(
            db_connection_list_head, 
            db_serial_id);

        return E_SQL_EXECUTE;
    }
	
    // bulk-insert location information to temp table
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_bulk_insert, filename);
    ret_val = SQL_execute(db_conn, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        remove(filename);
        SQL_rollback_transaction(db_conn);

        SQL_release_database_connection(
            db_connection_list_head, 
            db_serial_id);
        return E_SQL_EXECUTE;
    }

    // use temp table to update object_summary_table
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_update);
    ret_val = SQL_execute(db_conn, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        remove(filename);
        SQL_rollback_transaction(db_conn);

        SQL_release_database_connection(
            db_connection_list_head, 
            db_serial_id);
        return E_SQL_EXECUTE;
    }

    // drop temp table
    memset(sql, 0, sizeof(sql));
    sprintf(sql, drop_temp);
    ret_val = SQL_execute(db_conn, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        remove(filename);
        SQL_rollback_transaction(db_conn);

        SQL_release_database_connection(
            db_connection_list_head, 
            db_serial_id);
        return E_SQL_EXECUTE;
    }

    // commit transaction
    SQL_commit_transaction(db_conn);

    SQL_release_database_connection(
        db_connection_list_head, 
        db_serial_id);

    remove(filename);

	return WORK_SUCCESSFULLY;	
}
	
ErrorCode SQL_upload_location_history(
    DBConnectionListHead *db_connection_list_head,
    char* filename){

    char* sql_template_for_history_table=
        "COPY " \
        "location_history_table " \
        "(mac_address , " \
        "uuid , " \
        "record_timestamp , " \
        "battery_voltage , " \
        "average_rssi , " \
        "base_x , " \
        "base_y) " \
        "FROM " \
        "\'%s\' " \
        "DELIMITER \',\' CSV; ";

    PGconn *db_conn = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
	

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot open database\n");

        return E_SQL_OPEN_DATABASE;
    }

    /* Execute SQL statement */
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template_for_history_table, filename);
    ret_val = SQL_execute(db_conn, sql);
	
    SQL_release_database_connection(
        db_connection_list_head, 
        db_serial_id);

    remove(filename);

    if(WORK_SUCCESSFULLY != ret_val){
        return E_SQL_EXECUTE;
    }
	
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_panic_status(
    DBConnectionListHead *db_connection_list_head,
    char* object_mac_address){

    char *sql_identify_panic = 
        "UPDATE object_summary_table " \
        "SET panic_violation_timestamp = NOW() " \
        "FROM object_table " \
        "WHERE object_summary_table.mac_address = %s " \
        "AND object_summary_table.mac_address = object_table.mac_address " \
        "AND object_table.monitor_type & %d = %d;";

    PGconn *db_conn = NULL;
    char* pqescape_mac_address = NULL;
    int db_serial_id = -1;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
	

    if(WORK_SUCCESSFULLY != 
       SQL_get_database_connection(db_connection_list_head, 
                                   &db_conn, 
                                   &db_serial_id)){
        zlog_error(category_debug,
                   "cannot open database\n");

        return E_SQL_OPEN_DATABASE;
    }
	
    pqescape_mac_address = 
        PQescapeLiteral(db_conn, object_mac_address, 
                        strlen(object_mac_address));

    /* Execute SQL statement */
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_identify_panic, 
            pqescape_mac_address,
            MONITOR_PANIC,
            MONITOR_PANIC);
	
    PQfreemem(pqescape_mac_address);

    ret_val = SQL_execute(db_conn, sql);

    SQL_release_database_connection(
        db_connection_list_head, 
        db_serial_id);

    if(WORK_SUCCESSFULLY != ret_val){
        return E_SQL_EXECUTE;
    }
	
	return WORK_SUCCESSFULLY;
}