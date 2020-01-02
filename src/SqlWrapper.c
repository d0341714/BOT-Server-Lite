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


static ErrorCode SQL_execute(void *db, char *sql_statement){

    PGconn *conn = (PGconn *) db;
    PGresult *res;

    zlog_info(category_debug, "SQL command = [%s]", sql_statement);

    res = PQexec(conn, sql_statement);

    if(PQresultStatus(res) != PGRES_COMMAND_OK){

        zlog_error(category_debug, 
                   "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        PQclear(res);
        return E_SQL_EXECUTE;
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_begin_transaction(void* db){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "BEGIN TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_commit_transaction(void *db){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "END TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_rollback_transaction(void *db){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "ROLLBACK;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_open_database_connection(char *conninfo, void **db){

    *db = (PGconn*) PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(*db) != CONNECTION_OK)
    {
        zlog_info(category_debug, 
                  "Connection to database failed: %s", 
                  PQerrorMessage(*db));

        return E_SQL_OPEN_DATABASE;
    }

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_close_database_connection(void *db){

    PGconn *conn = (PGconn *) db;
    PQfinish(conn);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_vacuum_database(void *db){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *table_name[] = {"tracking_table",
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
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            return E_SQL_EXECUTE;
        }

    }

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_delete_old_data(void *db, int retention_hours){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *table_name[] = {"notification_table"};
    char *sql_template = "DELETE FROM %s WHERE " \
                         "violation_timestamp < " \
                         "NOW() - INTERVAL \'%d HOURS\';";
    int idx = 0;
    char *tsdb_table_name[] = {"tracking_table"};
    char *sql_tsdb_template = "SELECT drop_chunks(interval \'%d HOURS\', " \
                              "\'%s\');";
    PGresult *res;


    //SQL_begin_transaction(db);

    for(idx = 0; idx< sizeof(table_name)/sizeof(table_name[0]); idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_template, table_name[idx], retention_hours);

        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            //SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    for(idx = 0; 
        idx< sizeof(tsdb_table_name)/sizeof(tsdb_table_name[0]); 
        idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_tsdb_template, retention_hours, 
                tsdb_table_name[idx]);

        /* Execute SQL statement */

        zlog_info(category_debug, "SQL command = [%s]", sql);

        res = PQexec(conn, sql);
        if(PQresultStatus(res) != PGRES_TUPLES_OK){

            PQclear(res);
            zlog_info(category_debug, "SQL_execute failed: %s", 
                      PQerrorMessage(conn));
            //SQL_rollback_transaction(db);

            return E_SQL_EXECUTE;

        }
        PQclear(res);
    }

   //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_gateway_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char *numbers_str = NULL;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "INSERT INTO gateway_table " \
                         "(ip_address, " \
                         "health_status, " \
                         "registered_timestamp, " \
                         "last_report_timestamp) " \
                         "VALUES " \
                         "(%s, \'%d\', NOW(), NOW())" \
                         "ON CONFLICT (ip_address) " \
                         "DO UPDATE SET health_status = \'%d\', " \
                         "last_report_timestamp = NOW();";
    HealthStatus health_status = S_NORMAL_STATUS;
    char *ip_address = NULL;
    char *pqescape_ip_address = NULL;


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

    //SQL_begin_transaction(db);

    while( numbers-- ){
        
        ip_address = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
       
        /* Create SQL statement */
        pqescape_ip_address =
            PQescapeLiteral(conn, ip_address, strlen(ip_address));

        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template,
                pqescape_ip_address,
                health_status, health_status);

        PQfreemem(pqescape_ip_address);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            //SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_lbeacon_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len,
                                                 char *gateway_ip_address){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char *numbers_str = NULL;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "INSERT INTO lbeacon_table " \
                         "(uuid, " \
                         "ip_address, " \
                         "health_status, " \
                         "gateway_ip_address, " \
                         "registered_timestamp, " \
                         "last_report_timestamp) " \
                         "VALUES " \
                         "(%s, %s, \'%d\', %s, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
                         "NOW()) " \
                         "ON CONFLICT (uuid) " \
                         "DO UPDATE SET ip_address = %s, " \
                         "health_status = \'%d\', " \
                         "gateway_ip_address = %s, " \
                         "last_report_timestamp = NOW() ;";
    HealthStatus health_status = S_NORMAL_STATUS;
    char *uuid = NULL;
    char *lbeacon_ip = NULL;
    char *not_used_gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;
    char *pqescape_uuid = NULL;
    char *pqescape_lbeacon_ip = NULL;
    char *pqescape_gateway_ip = NULL;
    char *pqescape_registered_timestamp_GMT = NULL;


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

    //SQL_begin_transaction(db);

    while( numbers-- ){
        uuid = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        registered_timestamp_GMT = 
            strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
        lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));

        pqescape_uuid = PQescapeLiteral(conn, uuid, strlen(uuid));
        pqescape_lbeacon_ip =
            PQescapeLiteral(conn, lbeacon_ip, strlen(lbeacon_ip));
        pqescape_gateway_ip =
            PQescapeLiteral(conn, gateway_ip_address, 
                            strlen(gateway_ip_address));
        pqescape_registered_timestamp_GMT =
            PQescapeLiteral(conn, registered_timestamp_GMT,
                            strlen(registered_timestamp_GMT));

        sprintf(sql, sql_template,
                pqescape_uuid,
                pqescape_lbeacon_ip,
                health_status,
                pqescape_gateway_ip,
                pqescape_registered_timestamp_GMT,
                pqescape_lbeacon_ip,
                health_status,
                pqescape_gateway_ip);

        PQfreemem(pqescape_uuid);
        PQfreemem(pqescape_lbeacon_ip);
        PQfreemem(pqescape_gateway_ip);
        PQfreemem(pqescape_registered_timestamp_GMT);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            //SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_gateway_health_status(void *db,
                                           char *buf,
                                           size_t buf_len,
                                           char *gateway_ip_address){

    PGconn *conn = (PGconn *) db;
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

    //SQL_begin_transaction(db);

    /* Create SQL statement */
    pqescape_ip_address =
        PQescapeLiteral(conn, gateway_ip_address, strlen(gateway_ip_address));
    pqescape_health_status =
        PQescapeLiteral(conn, health_status, strlen(health_status));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template,
            pqescape_health_status,
            pqescape_ip_address);

    PQfreemem(pqescape_ip_address);
    PQfreemem(pqescape_health_status);

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        //SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_lbeacon_health_status(void *db,
                                           char *buf,
                                           size_t buf_len,
                                           char *gateway_ip_address){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "UPDATE lbeacon_table " \
                         "SET health_status = %s, " \
                         "last_report_timestamp = NOW(), " \
                         "gateway_ip_address = %s " \
                         "WHERE uuid = %s ;";
    char *lbeacon_uuid = NULL;
    char *lbeacon_timestamp = NULL;
    char *lbeacon_ip = NULL;
    char *health_status = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_health_status = NULL;
    char *pqescape_gateway_ip = NULL;


    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    health_status = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    //SQL_begin_transaction(db);

    /* Create SQL statement */
    pqescape_lbeacon_uuid = 
        PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));
    pqescape_health_status =
        PQescapeLiteral(conn, health_status, strlen(health_status));
    pqescape_gateway_ip = 
        PQescapeLiteral(conn, gateway_ip_address, strlen(gateway_ip_address));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template,
            pqescape_health_status,
            pqescape_gateway_ip,
            pqescape_lbeacon_uuid);

    PQfreemem(pqescape_lbeacon_uuid);
    PQfreemem(pqescape_health_status);
    PQfreemem(pqescape_gateway_ip);

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        //SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}

/*
ErrorCode SQL_update_object_tracking_data(void *db,
                                          char *buf,
                                          size_t buf_len){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int num_types = 2; // BR_EDR and BLE types
    char *sql_template = "INSERT INTO tracking_table " \
                         "(object_mac_address, " \
                         "lbeacon_uuid, " \
                         "rssi, " \
                         "panic_button, " \
                         "initial_timestamp, " \
                         "final_timestamp, " \
                         "server_time_offset) " \
                         "VALUES " \
                         "(%s, %s, %s, %s, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, "
                         "%d);";
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


    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
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

            initial_timestamp_GMT = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            final_timestamp_GMT = 
                strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            rssi = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            push_button = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

            pqescape_object_mac_address =
                PQescapeLiteral(conn, object_mac_address,
                                strlen(object_mac_address));
            pqescape_lbeacon_uuid =
                PQescapeLiteral(conn, lbeacon_uuid, 
                                strlen(lbeacon_uuid));
            pqescape_rssi = 
                PQescapeLiteral(conn, rssi, strlen(rssi));
            pqescape_push_button =
                PQescapeLiteral(conn, push_button, 
                                strlen(push_button));
            pqescape_initial_timestamp_GMT =
                PQescapeLiteral(conn, initial_timestamp_GMT,
                                strlen(initial_timestamp_GMT));
            pqescape_final_timestamp_GMT =
                PQescapeLiteral(conn, final_timestamp_GMT,
                                strlen(final_timestamp_GMT));

            memset(sql, 0, sizeof(sql));
            sprintf(sql, sql_template,
                    pqescape_object_mac_address,
                    pqescape_lbeacon_uuid,
                    pqescape_rssi,
                    pqescape_push_button,
                    pqescape_initial_timestamp_GMT,
                    pqescape_final_timestamp_GMT,
                    current_time - lbeacon_timestamp_value);

            PQfreemem(pqescape_object_mac_address);
            PQfreemem(pqescape_lbeacon_uuid);
            PQfreemem(pqescape_rssi);
            PQfreemem(pqescape_push_button);
            PQfreemem(pqescape_initial_timestamp_GMT);
            PQfreemem(pqescape_final_timestamp_GMT);

            ret_val = SQL_execute(db, sql);

            if(WORK_SUCCESSFULLY != ret_val){
                //SQL_rollback_transaction(db);
                return E_SQL_EXECUTE;
            }
        }
    }

    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}
*/


ErrorCode SQL_update_object_tracking_data_with_battery_voltage(void *db,
                                                               char *buf,
                                                               size_t buf_len,
                                                               char *server_installation_path,
                                                               int is_enabled_panic_monitoring){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *saveptr = NULL;
    int num_types = 2; // BR_EDR and BLE types
    char *sql_bulk_insert_template = 
                         "COPY " \
                         "tracking_table " \
                         "(object_mac_address, " \
                         "lbeacon_uuid, " \
                         "rssi, " \
                         "panic_button, " \
                         "battery_voltage, " \
                         "initial_timestamp, " \
                         "final_timestamp, " \
                         "server_time_offset) " \
                         "FROM " \
                         "\'%s\' " \
                         "DELIMITER \',\' CSV;";
    
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
    char *battery_voltage = NULL;
    int current_time = get_system_time();
    int lbeacon_timestamp_value;
    char filename[MAX_PATH];
    FILE *file = NULL;
    time_t rawtime;
    struct tm ts;
    char buf_initial_time[80];
    char buf_final_time[80];

    char *sql_identify_panic = 
        "UPDATE object_summary_table " \
        "SET panic_violation_timestamp = NOW() " \
        "FROM object_summary_table as R " \
        "INNER JOIN object_table " \
        "ON R.mac_address = object_table.mac_address " \
        "WHERE object_summary_table.mac_address = %s " \
        "AND object_table.monitor_type & %d = %d;";
    char *pqescape_mac_address = NULL;
   
    /* Open temporary file with thread id as filename to prepare the tracking 
       data for postgresql bulk-insertion */
    memset(filename, 0, sizeof(filename));
    sprintf(filename, "%s/temp/track_%d", 
            server_installation_path, 
            pthread_self());
    
    file = fopen(filename, "wt");
    if(file == NULL){
        zlog_error(category_debug, "cannot open filepath %s", filename);
        return E_OPEN_FILE;
    }

    /* Parse the message buffer */
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    if(lbeacon_timestamp == NULL){
        fclose(file);
        return E_API_PROTOCOL_FORMAT;
    }
    lbeacon_timestamp_value = atoi(lbeacon_timestamp);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    zlog_debug(category_debug, "lbeacon_uuid=[%s], lbeacon_timestamp=[%s], " \
               "lbeacon_ip=[%s]", lbeacon_uuid, lbeacon_timestamp, lbeacon_ip);

    while(num_types --){

        object_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        object_number = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        zlog_debug(category_debug, "object_type=[%s], object_number=[%s]", 
                   object_type, object_number);

        if(object_number == NULL){
            fclose(file);
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

            if(panic_button != NULL && 1 == atoi(panic_button)){
                pqescape_mac_address = 
                    PQescapeLiteral(conn, object_mac_address, 
                                    strlen(object_mac_address)); 

                memset(sql, 0, sizeof(sql));
                sprintf(sql, sql_identify_panic, 
                        pqescape_mac_address, 
                        MONITOR_PANIC,
                        MONITOR_PANIC);

                ret_val = SQL_execute(db, sql);

                PQfreemem(pqescape_mac_address);
            }

            battery_voltage = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
           
            // Convert Unix epoch timestamp (since 1970-1-1) to 
            // postgre timestamp (since 2000-1-1)
            rawtime = atoi(initial_timestamp_GMT);
            ts = *gmtime(&rawtime);
            strftime(buf_initial_time, sizeof(buf_initial_time), 
                     "%Y-%m-%d %H:%M:%S", &ts);
            
            rawtime = atoi(final_timestamp_GMT);
            ts = *gmtime(&rawtime);
            strftime(buf_final_time, sizeof(buf_final_time), 
                     "%Y-%m-%d %H:%M:%S", &ts);
                      
            fprintf(file, "%s,%s,%s,%s,%s,%s,%s,%d\n",
                    object_mac_address,
                    lbeacon_uuid,
                    rssi,
                    panic_button,
                    battery_voltage,
                    buf_initial_time,
                    buf_final_time,
                    current_time - lbeacon_timestamp_value);
        }
    }
    fclose(file);
    
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_bulk_insert_template, filename); 

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    remove(filename);

    if(WORK_SUCCESSFULLY != ret_val){
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_summarize_object_location(void *db, 
                                        int database_pre_filter_time_window_in_sec,
                                        int time_interval_in_sec){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_select_template = "UPDATE object_summary_table " \
                                "SET " \
                                "first_seen_timestamp = CASE " \
                                "WHEN first_seen_timestamp IS NULL OR " \
                                "object_summary_table.uuid != " \
                                "location_information.lbeacon_uuid " \
                                "THEN " \
                                "location_information.initial_timestamp " \
                                "ELSE first_seen_timestamp " \
                                "END, " \
                                "rssi = location_information.avg_rssi, " \
                                "battery_voltage = " \
                                "location_information.battery_voltage, " \
                                "last_seen_timestamp = " \
                                "location_information.final_timestamp, " \
                                "uuid = location_information.lbeacon_uuid " \
                                "FROM " \
                                "(SELECT " \
                                "object_mac_address, " \
                                "lbeacon_uuid, " \
                                "avg_rssi, " \
                                "battery_voltage, " \
                                "initial_timestamp, " \
                                "final_timestamp " \
                                "FROM " \
                                "(SELECT " \
                                "ROW_NUMBER() OVER (" \
                                "PARTITION BY object_mac_address " \
                                "ORDER BY object_mac_address ASC, avg_rssi DESC" \
                                ") as rank, " \
                                "object_beacon_rssi_table.* " \
                                "FROM "\
                                "(SELECT " \
                                "t.object_mac_address, " \
                                "t.lbeacon_uuid, " \
                                "ROUND(AVG(rssi), 0) as avg_rssi, " \
                                "MIN(battery_voltage) as battery_voltage, " \
                                "MIN(initial_timestamp) as initial_timestamp, " \
                                "MAX(final_timestamp) as final_timestamp " \
                                "FROM " \
                                "tracking_table t " \
                                "WHERE " \
                                "final_timestamp >= " \
                                "NOW() - INTERVAL '%d seconds' AND " \
                                "final_timestamp >= " \
                                "NOW() - (server_time_offset || 'seconds')::INTERVAL - " \
                                "INTERVAL '%d seconds' " \
                                "GROUP BY " \
                                "object_mac_address, " \
                                "lbeacon_uuid " \
                                "HAVING AVG(rssi) > -100 " \
                                "ORDER BY " \
                                "object_mac_address ASC, " \
                                "avg_rssi DESC, " \
                                "lbeacon_uuid ASC" \
                                ") object_beacon_rssi_table " \
                                ") object_location_table " \
                                "WHERE " \
                                "object_location_table.rank <= 1 " \
                                ") location_information " \
                                "WHERE " \
                                "object_summary_table.mac_address = " \
                                "location_information.object_mac_address;";

    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            database_pre_filter_time_window_in_sec, 
            time_interval_in_sec);

    ret_val = SQL_execute(db, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_geofence_violation(void *db, char *mac_address){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_insert_summary_table = 
        "UPDATE object_summary_table " \
        "SET " \
        "geofence_violation_timestamp = NOW() " \
        "WHERE mac_address = %s";

    char *pqescape_mac_address = NULL;

    memset(sql, 0, sizeof(sql));

    pqescape_mac_address = 
        PQescapeLiteral(conn, mac_address, 
                        strlen(mac_address)); 
   
    sprintf(sql, 
            sql_insert_summary_table, 
            pqescape_mac_address);
            
    //SQL_begin_transaction(db);

    ret_val = SQL_execute(db, sql);

    PQfreemem(pqescape_mac_address);
   
    if(WORK_SUCCESSFULLY != ret_val){
        //SQL_rollback_transaction(db);
        
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }     
    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_location_not_stay_room(void *db){

    PGconn *conn = (PGconn *)db;
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
                                "AND lbeacon_table.room <> object_table.room " \
                                ") location_information " \
                                "WHERE object_summary_table.mac_address = " \
                                "location_information.mac_address;";

    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            MONITOR_LOCATION,
            MONITOR_LOCATION);

    ret_val = SQL_execute(db, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }
    
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_location_long_stay_in_danger(void *db){

    PGconn *conn = (PGconn *)db;
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

    ret_val = SQL_execute(db, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_last_movement_status(void *db, 
                                            int time_interval_in_min, 
                                            int each_time_slot_in_min,
                                            unsigned int rssi_delta){

    PGconn *conn = (PGconn *)db;
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
        "SELECT TIME_BUCKET('%d minutes', final_timestamp) as time_slot, " \
        "AVG(rssi) as avg_rssi " \
        "FROM tracking_table where " \
        "final_timestamp > NOW() - INTERVAL '%d minutes' " \
        "AND lbeacon_uuid = %s " \
        "AND object_mac_address = %s " \
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

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){

        //SQL_begin_transaction(db);

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
                PQescapeLiteral(conn, mac_address, strlen(mac_address));
            pqescape_lbeacon_uuid = 
                PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));

            sprintf(sql, sql_select_activity_template, 
                    each_time_slot_in_min,
                    time_interval_in_min,
                    pqescape_lbeacon_uuid,
                    pqescape_mac_address,
                    rssi_delta,
                    0 - rssi_delta);

            res_activity = PQexec(conn, sql);

            PQfreemem(pqescape_mac_address);
            PQfreemem(pqescape_lbeacon_uuid);

            if(PQresultStatus(res_activity) != PGRES_TUPLES_OK){
                PQclear(res_activity);
                //SQL_rollback_transaction(db);
                PQclear(res);
                    
                zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                           res_activity, PQerrorMessage(conn));

                return E_SQL_EXECUTE;
            }

            rows_activity = PQntuples(res_activity);
         
            if(rows_activity == 0){
                pqescape_mac_address = 
                    PQescapeLiteral(conn, mac_address, 
                                    strlen(mac_address));
                
                memset(sql, 0, sizeof(sql));
                    
                sprintf(sql, sql_update_activity_template,
                        pqescape_mac_address);
                            
                ret_val = SQL_execute(db, sql);

                PQfreemem(pqescape_mac_address);
               
                if(WORK_SUCCESSFULLY != ret_val){
                    PQclear(res_activity);   
                    //SQL_rollback_transaction(db);
                    PQclear(res);

                    zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                               ret_val, PQerrorMessage(conn));

                    return E_SQL_EXECUTE;
                }     
                PQclear(res_activity);

                continue;
            }
            PQclear(res_activity);
        }

        //SQL_commit_transaction(db);
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_collect_violation_events(
    void *db, 
    ObjectMonitorType monitor_type,
    int time_interval_in_sec,
    int granularity_for_continuous_violations_in_sec){

    PGconn *conn = (PGconn *)db;
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

    //SQL_begin_transaction(db);
    ret_val = SQL_execute(db, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        //SQL_rollback_transaction(db);
        
        zlog_error(category_debug, 
                   "SQL_execute failed [%d]: %s", 
                   ret_val, 
                   PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }     
    //SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_get_and_update_violation_events(void *db, 
                                              char *buf, 
                                              size_t buf_len){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_select_template = 
        "SELECT id, monitor_type, mac_address, uuid, violation_timestamp " \
        "FROM "
        "notification_table " \
        "WHERE "\
        "processed != 1 " \
        "ORDER BY id ASC;";
    const int NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE = 5;
    const int FIELD_INDEX_OF_ID = 0;
    const int FIELD_INDEX_OF_MONITOR_TYPE = 1;
    const int FIELD_INDEX_OF_MAC_ADDRESS = 2;
    const int FIELD_INDEX_OF_UUID = 3;
    const int FIELD_INDEX_OF_VIOLATION_TIMESTAMP = 4;

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
    sprintf(sql, sql_select_template);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_rows = PQntuples(res);
    total_fields = PQnfields(res);
    
    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){
        for(i = 0 ; i < total_rows ; i++){
            memset(one_record, 0, sizeof(one_record));
            sprintf(one_record, "%s,%s,%s,%s,%s;", 
                    PQgetvalue(res, i, FIELD_INDEX_OF_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_MONITOR_TYPE),
                    PQgetvalue(res, i, FIELD_INDEX_OF_MAC_ADDRESS),
                    PQgetvalue(res, i, FIELD_INDEX_OF_UUID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_VIOLATION_TIMESTAMP));
            
            if(buf_len > strlen(buf) + strlen(one_record)){
                strcat(buf, one_record);
            
                memset(sql, 0, sizeof(sql));
                if(PQgetvalue(res, i, FIELD_INDEX_OF_ID) == NULL){
                    PQclear(res);
                    return E_API_PROTOCOL_FORMAT;
                }
                sprintf(sql, 
                        sql_update_template, 
                        atoi(PQgetvalue(res, i, FIELD_INDEX_OF_ID)));

                ret_val = SQL_execute(db, sql);        
            }
        }
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_reload_monitor_config(void *db,
                                    int server_localtime_against_UTC_in_hour)
{
    PGconn *conn = (PGconn *) db;
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

        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
        //SQL_rollback_transaction(db);
        
        zlog_error(category_debug, 
                   "SQL_execute failed [%d]: %s", 
                   ret_val, 
                   PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }     
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_dump_active_geo_fence_settings(void *db, char *filename)
{
    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    
    char *sql_select_template = "SELECT " \
                                "area_id, " \
                                "id, " \
                                "name, " \
                                "perimeters, " \
                                "fences " \
                                "FROM geo_fence_config " \
                                "WHERE " \
                                "is_active = 1;";

    const int NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE = 5;
    const int FIELD_INDEX_OF_AREA_ID = 0;
    const int FIELD_INDEX_OF_ID = 1;
    const int FIELD_INDEX_OF_NAME = 2;
    const int FIELD_INDEX_OF_PERIMETRS = 3;
    const int FIELD_INDEX_OF_FENCES = 4;

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

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        fclose(file);

        return E_SQL_EXECUTE;
    }

    total_rows = PQntuples(res);
    total_fields = PQnfields(res);
    
    if(total_rows > 0 && 
       total_fields == NUMBER_FIELDS_OF_SQL_SELECT_TEMPLATE){
         
        for(i = 0 ; i < total_rows ; i++){
            fprintf(file, "%s;%s;%s;%s;%s;\n", 
                    PQgetvalue(res, i, FIELD_INDEX_OF_AREA_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_ID),
                    PQgetvalue(res, i, FIELD_INDEX_OF_NAME),
                    PQgetvalue(res, i, FIELD_INDEX_OF_PERIMETRS),
                    PQgetvalue(res, i, FIELD_INDEX_OF_FENCES));
        }
    }


    PQclear(res);
    fclose(file);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_dump_mac_address_under_geo_fence_monitor(void *db, char *filename){

    PGconn *conn = (PGconn *)db;
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

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

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
    fclose(file);

    return WORK_SUCCESSFULLY;

}