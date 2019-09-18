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

     1.0, 20190726

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

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", res, PQerrorMessage(conn));

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
        zlog_info(category_debug, "Connection to database failed: %s", PQerrorMessage(*db));

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
    char *table_name[5] = {"tracking_table",
                           "lbeacon_table",
                           "gateway_table",
                           "object_table",
                           "geo_fence_alert"};
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "VACUUM %s;";
    int idx = 0;

    for(idx = 0; idx< 5 ; idx++){

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


ErrorCode SQL_retain_data(void *db, int retention_hours){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *table_name[1] = {"geo_fence_alert"};
    char *sql_template = "DELETE FROM %s WHERE " \
                         "receive_time < " \
                         "NOW() - INTERVAL \'%d HOURS\';";
    int idx = 0;
    char *tsdb_table_name[1] = {"tracking_table"};
    char *sql_tsdb_template = "SELECT drop_chunks(interval \'%d HOURS\', " \
                              "\'%s\');";
    PGresult *res;


    SQL_begin_transaction(db);

    for(idx = 0; idx< 1; idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_template, table_name[idx], retention_hours);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    for(idx = 0; idx<1; idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_tsdb_template, retention_hours, tsdb_table_name[idx]);

        /* Execute SQL statement */

        zlog_info(category_debug, "SQL command = [%s]", sql);

        res = PQexec(conn, sql);
        if(PQresultStatus(res) != PGRES_TUPLES_OK){

            PQclear(res);
            zlog_info(category_debug, "SQL_execute failed: %s", PQerrorMessage(conn));
            SQL_rollback_transaction(db);

            return E_SQL_EXECUTE;

        }
        PQclear(res);
    }

    SQL_commit_transaction(db);

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
    numbers = atoi(numbers_str);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    SQL_begin_transaction(db);

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
            SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    SQL_commit_transaction(db);

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
    numbers = atoi(numbers_str);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    not_used_gateway_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    SQL_begin_transaction(db);

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
            PQescapeLiteral(conn, gateway_ip_address, strlen(gateway_ip_address));
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
            SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    SQL_commit_transaction(db);

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

    SQL_begin_transaction(db);

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
        SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    SQL_commit_transaction(db);

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

    SQL_begin_transaction(db);

    /* Create SQL statement */
    pqescape_lbeacon_uuid = PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));
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
        SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


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
    char *pqescape_object_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_rssi = NULL;
    char *pqescape_push_button = NULL;
    char *pqescape_initial_timestamp_GMT = NULL;
    char *pqescape_final_timestamp_GMT = NULL;


    zlog_debug(category_debug, "buf=[%s]", buf);

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    zlog_debug(category_debug, "lbeacon_uuid=[%s], lbeacon_timestamp=[%s], " \
               "lbeacon_ip=[%s]", lbeacon_uuid, lbeacon_timestamp, lbeacon_ip);

    SQL_begin_transaction(db);

    while(num_types --){

        object_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        object_number = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        zlog_debug(category_debug, "object_type=[%s], object_number=[%s]", 
                   object_type, object_number);

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

            zlog_debug(category_debug, "object_mac_address=[%s], rssi=[%s], " \
                       "push_button=[%s]", object_mac_address, rssi, 
                       push_button);

            /* Create SQL statement */
            pqescape_object_mac_address =
                PQescapeLiteral(conn, object_mac_address,
                                strlen(object_mac_address));
            pqescape_lbeacon_uuid =
                PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));
            pqescape_rssi = PQescapeLiteral(conn, rssi, strlen(rssi));
            pqescape_push_button =
                PQescapeLiteral(conn, push_button, strlen(push_button));
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
                    current_time - atoi(lbeacon_timestamp));

            PQfreemem(pqescape_object_mac_address);
            PQfreemem(pqescape_lbeacon_uuid);
            PQfreemem(pqescape_rssi);
            PQfreemem(pqescape_push_button);
            PQfreemem(pqescape_initial_timestamp_GMT);
            PQfreemem(pqescape_final_timestamp_GMT);

            /* Execute SQL statement */
            ret_val = SQL_execute(db, sql);

            if(WORK_SUCCESSFULLY != ret_val){
                SQL_rollback_transaction(db);
                return E_SQL_EXECUTE;
            }
        }
    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_object_tracking_data_with_battery_voltage(void *db,
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
                         "battery_voltage, " \
                         "initial_timestamp, " \
                         "final_timestamp, " \
                         "server_time_offset) " \
                         "VALUES " \
                         "(%s, %s, %s, %s, %s," \
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
    char *panic_button = NULL;
    char *battery_voltage = NULL;
    int current_time = get_system_time();
    char *pqescape_object_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_rssi = NULL;
    char *pqescape_panic_button = NULL;
    char *pqescape_battery_voltage = NULL;
    char *pqescape_initial_timestamp_GMT = NULL;
    char *pqescape_final_timestamp_GMT = NULL;


    zlog_debug(category_debug, "buf=[%s]", buf);

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = strtok_save(temp_buf, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_timestamp = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);
    lbeacon_ip = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

    zlog_debug(category_debug, "lbeacon_uuid=[%s], lbeacon_timestamp=[%s], " \
               "lbeacon_ip=[%s]", lbeacon_uuid, lbeacon_timestamp, lbeacon_ip);

    SQL_begin_transaction(db);

    while(num_types --){

        object_type = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        object_number = strtok_save(NULL, DELIMITER_SEMICOLON, &saveptr);

        zlog_debug(category_debug, "object_type=[%s], object_number=[%s]", 
                   object_type, object_number);

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

            zlog_debug(category_debug, "object_mac_address=[%s], rssi=[%s], " \
                       "panic_button=[%s], battery_voltage=[%s]", 
                       object_mac_address, rssi, panic_button, battery_voltage);

            /* Create SQL statement */
            pqescape_object_mac_address =
                PQescapeLiteral(conn, object_mac_address,
                                strlen(object_mac_address));
            pqescape_lbeacon_uuid =
                PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));
            pqescape_rssi = PQescapeLiteral(conn, rssi, strlen(rssi));
            pqescape_panic_button =
                PQescapeLiteral(conn, panic_button, strlen(panic_button));
            pqescape_battery_voltage =
                PQescapeLiteral(conn, battery_voltage, strlen(battery_voltage));
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
                    pqescape_panic_button,
                    pqescape_battery_voltage,
                    pqescape_initial_timestamp_GMT,
                    pqescape_final_timestamp_GMT,
                    current_time - atoi(lbeacon_timestamp));

            PQfreemem(pqescape_object_mac_address);
            PQfreemem(pqescape_lbeacon_uuid);
            PQfreemem(pqescape_rssi);
            PQfreemem(pqescape_panic_button);
            PQfreemem(pqescape_battery_voltage);
            PQfreemem(pqescape_initial_timestamp_GMT);
            PQfreemem(pqescape_final_timestamp_GMT);

            /* Execute SQL statement */
            ret_val = SQL_execute(db, sql);

            if(WORK_SUCCESSFULLY != ret_val){
                SQL_rollback_transaction(db);
                return E_SQL_EXECUTE;
            }
        }
    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_insert_geo_fence_alert(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *saved_ptr;
    char content_temp[WIFI_MESSAGE_LENGTH];
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "INSERT INTO geo_fence_alert " \
                         "(mac_address, " \
                         "name, " \
                         "type, " \
                         "uuid, " \
                         "alert_time, " \
                         "rssi, " \
                         "receive_time) " \
                         "VALUES " \
                         "(%s, %s, %s, %s, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
                         "%s, NOW());";

    int number_of_geo_fence_alert = 0;
    char *number_of_geo_fence_alert_str = NULL;
    char *mac_address = NULL;
    char *name = NULL;
    char *type = NULL;
    char *uuid = NULL;
    char *alert_time = NULL;
    char *rssi = NULL;

    char *pqescape_mac_address = NULL;
    char *pqescape_name = NULL;
    char *pqescape_type = NULL;
    char *pqescape_uuid = NULL;
    char *pqescape_alert_time = NULL;
    char *pqescape_rssi = NULL;


    saved_ptr = NULL;

    memset(content_temp, 0, WIFI_MESSAGE_LENGTH);
    memcpy(content_temp, buf, buf_len);

    number_of_geo_fence_alert_str = strtok_save(content_temp, 
                                                DELIMITER_SEMICOLON, 
                                                 &saved_ptr);
    sscanf(number_of_geo_fence_alert_str, "%d", &number_of_geo_fence_alert);

    SQL_begin_transaction(db);

    while( number_of_geo_fence_alert-- ){

        mac_address = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        name = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        type = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        uuid = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        alert_time = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        rssi = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

        /* Create SQL statement */
        memset(sql, 0, strlen(sql) * sizeof(char));

        pqescape_mac_address = PQescapeLiteral(conn, mac_address,
                                               strlen(mac_address));
        pqescape_name = PQescapeLiteral(conn, name, strlen(name));
        pqescape_type = PQescapeLiteral(conn, type, strlen(type));
        pqescape_uuid = PQescapeLiteral(conn, uuid, strlen(uuid));
        pqescape_alert_time = PQescapeLiteral(conn, alert_time,
                                              strlen(alert_time));
        pqescape_rssi = PQescapeLiteral(conn, rssi, strlen(rssi));


        sprintf(sql, sql_template,
                pqescape_mac_address,
                pqescape_name,
                pqescape_type,
                pqescape_uuid,
                pqescape_alert_time,
                pqescape_rssi);

        PQfreemem(pqescape_mac_address);
        PQfreemem(pqescape_name);
        PQfreemem(pqescape_type);
        PQfreemem(pqescape_uuid);
        PQfreemem(pqescape_alert_time);
        PQfreemem(pqescape_rssi);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            SQL_rollback_transaction(db);
            return E_SQL_EXECUTE;
        }

    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_summarize_object_location(void *db, int time_interval_in_sec){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "SELECT object_mac_address, lbeacon_uuid, " \
                                "ROUND( AVG(rssi), 2) as avg_rssi, " \
                                "MIN(battery_voltage) as battery_voltage, " \
                                "MIN(initial_timestamp) as initial_timestamp, " \
                                "MAX(final_timestamp) as final_timestamp " \
                                "FROM tracking_table " \
                                "WHERE final_timestamp >= " \
                                "NOW() - INTERVAL '%d seconds' AND " \
                                "final_timestamp >= NOW() - " \
                                "(server_time_offset|| 'seconds')::INTERVAL - " \
                                "INTERVAL '%d seconds' GROUP BY " \
                                "object_mac_address, lbeacon_uuid " \
                                "HAVING AVG(rssi) > -100 ORDER BY " \
                                "object_mac_address ASC, avg_rssi DESC";
    PGresult *res = NULL;
    int current_row = 0;
    int total_fields = 0;
    int total_rows = 0;

    char *mac_address = NULL;
    char prev_mac_address[LENGTH_OF_MAC_ADDRESS];

    char *lbeacon_uuid = NULL;
    char *avg_rssi = NULL;
    char *battery_voltage = NULL;
    char *initial_timestamp = NULL;
    char *final_timestamp = NULL;

    char *pqescape_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_rssi = NULL;
    char *pqescape_initial_timestamp = NULL;
    char *pqescape_final_timestamp = NULL;

    char *sql_select_mac_address_lbeacon_uuid_template = 
        "SELECT mac_address, uuid " \
        "from " \
        "object_summary_table where " \
        "mac_address = %s AND uuid = %s";

    PGresult *res_mac_address = NULL;
    int rows_mac_address = 0;
    int fields_mac_address = 0;

    char *sql_update_location_information_template = 
        "UPDATE object_summary_table " \
        "set uuid = %s, " \
        "rssi = %d, " \
        "battery_voltage = %d, " \
        "first_seen_timestamp = %s, " \
        "last_seen_timestamp = %s" \
        "WHERE mac_address = %s";

    char *sql_update_timing_template = 
        "UPDATE object_summary_table " \
        "set rssi = %d, " \
        "battery_voltage = %d, " \
        "last_seen_timestamp = %s " \
        "WHERE mac_address = %s";


    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            time_interval_in_sec + 5, 
            time_interval_in_sec);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    if(total_rows > 0 && total_fields == 6){

        memset(prev_mac_address, 0, sizeof(prev_mac_address));

        SQL_begin_transaction(db);

        for(current_row = 0 ; current_row < total_rows ; current_row++){

            mac_address = PQgetvalue(res, current_row, 0);

            // we only need to handle the first row of each pair of 
            // mac_address and lbeacon_uuid, because we have sorted 
            // the result by avg_rssi in the SQL command.
            if(0 != strncmp(prev_mac_address, 
                            mac_address, 
                            strlen(mac_address))){

                strncpy(prev_mac_address, mac_address, strlen(mac_address));

                lbeacon_uuid = PQgetvalue(res, current_row, 1);
                avg_rssi = PQgetvalue(res, current_row, 2);
                battery_voltage = PQgetvalue(res, current_row, 3);
                initial_timestamp = PQgetvalue(res, current_row, 4);
                final_timestamp = PQgetvalue(res, current_row, 5);

                zlog_debug(category_debug, "get location [%s] [%s] [%s]",
                           mac_address, lbeacon_uuid, avg_rssi);

                // first, check if the pair of mac_address and lbeacon_uuid 
                // exists in the object_summary_table.
                pqescape_mac_address = 
                    PQescapeLiteral(conn, mac_address, strlen(mac_address));
                pqescape_lbeacon_uuid = 
                        PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));
                
                sprintf(sql, sql_select_mac_address_lbeacon_uuid_template, 
                        pqescape_mac_address,
                        pqescape_lbeacon_uuid);

                res_mac_address = PQexec(conn, sql);

                PQfreemem(pqescape_mac_address);
                PQfreemem(pqescape_lbeacon_uuid);

                if(PQresultStatus(res_mac_address) != PGRES_TUPLES_OK){
                    PQclear(res_mac_address);
                    SQL_rollback_transaction(db);
                    PQclear(res);
                    
                    zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                               res_mac_address, PQerrorMessage(conn));

                    return E_SQL_EXECUTE;
                }

                rows_mac_address = PQntuples(res_mac_address);
                fields_mac_address = PQnfields(res_mac_address);

                if(rows_mac_address == 0){
                    // if the pair of mac_address and lbeacon_uuid does not exist
                    // in the object_summary_table, we update the whole location 
                    // information

                    pqescape_mac_address = 
                        PQescapeLiteral(conn, mac_address, 
                                        strlen(mac_address));
                    pqescape_lbeacon_uuid = 
                        PQescapeLiteral(conn, lbeacon_uuid, 
                                        strlen(lbeacon_uuid));
                    pqescape_initial_timestamp = 
                        PQescapeLiteral(conn, initial_timestamp, 
                                        strlen(initial_timestamp));
                    pqescape_final_timestamp = 
                        PQescapeLiteral(conn, final_timestamp, 
                                        strlen(final_timestamp));

                    memset(sql, 0, sizeof(sql));
                    sprintf(sql, sql_update_location_information_template,  
                                 pqescape_lbeacon_uuid,
                                 atoi(avg_rssi),
                                 atoi(battery_voltage),
                                 pqescape_initial_timestamp, 
                                 pqescape_final_timestamp,
                                 pqescape_mac_address);

                    SQL_execute(db, sql);

                    PQfreemem(pqescape_mac_address);
                    PQfreemem(pqescape_lbeacon_uuid);
                    PQfreemem(pqescape_initial_timestamp);
                    PQfreemem(pqescape_final_timestamp);

                    if(WORK_SUCCESSFULLY != ret_val){
                        PQclear(res_mac_address);   
                        SQL_rollback_transaction(db);
                        PQclear(res);

                        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                                   ret_val, PQerrorMessage(conn));

                        return E_SQL_EXECUTE;
                    }
                }
                else if(rows_mac_address == 1)
                {
                    if(fields_mac_address == 2){
                    // if the pair of mac_address and lbeacon_uuid exists
                    // in the object_summary_table, we update the timing 
                    // information
                       
                        pqescape_mac_address = 
                            PQescapeLiteral(conn, mac_address, 
                                            strlen(mac_address));
                        pqescape_final_timestamp = 
                            PQescapeLiteral(conn, final_timestamp, 
                                        strlen(final_timestamp));
                     
                        memset(sql, 0, sizeof(sql));
                        sprintf(sql, sql_update_timing_template,  
                                atoi(avg_rssi),
                                atoi(battery_voltage),
                                pqescape_final_timestamp,
                                pqescape_mac_address);

                        SQL_execute(db, sql);

                        PQfreemem(pqescape_mac_address);
                        PQfreemem(pqescape_final_timestamp);

                        if(WORK_SUCCESSFULLY != ret_val){
                            PQclear(res_mac_address);    
                            SQL_rollback_transaction(db);
                            PQclear(res);

                            zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                                       ret_val, PQerrorMessage(conn));

                            return E_SQL_EXECUTE;
                        }
                    }
                }
                PQclear(res_mac_address);                
            }
        }

        SQL_commit_transaction(db);
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_panic(void *db, int time_interval_in_sec){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "SELECT object_mac_address, lbeacon_uuid, " \
                                "panic_button, " \
                                "MAX(final_timestamp) as final_timestamp " \
                                "FROM tracking_table " \
                                "WHERE final_timestamp >= " \
                                "NOW() - INTERVAL '%d seconds' AND " \
                                "final_timestamp >= NOW() - " \
                                "(server_time_offset|| 'seconds')::INTERVAL - " \
                                "INTERVAL '%d seconds' AND " \
                                "panic_button = 1 " \
                                "GROUP BY " \
                                "object_mac_address, lbeacon_uuid, panic_button " \
                                "ORDER BY " \
                                "object_mac_address ASC";
    PGresult *res = NULL;
    int current_row = 0;
    int total_fields = 0;
    int total_rows = 0;

    char *mac_address;
    char prev_mac_address[LENGTH_OF_MAC_ADDRESS];

    char *pqescape_mac_address = NULL;

    char *final_timestamp = NULL;
    char *pqescape_final_timestamp = NULL;

    char *sql_update_panic_template = 
        "UPDATE object_summary_table " \
        "set panic_timestamp = %s " \
        "WHERE mac_address = %s";

    ObjectMonitorType object_monitor_type = MONITOR_NORMAL;

    
    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            time_interval_in_sec + 5, 
            time_interval_in_sec);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    if(total_rows > 0 && total_fields == 4){

        memset(prev_mac_address, 0, sizeof(prev_mac_address));
        
        SQL_begin_transaction(db);

        for(current_row = 0 ; current_row < total_rows ; current_row++){

            mac_address = PQgetvalue(res, current_row, 0);
            final_timestamp = PQgetvalue(res, current_row, 3);

            // we only need to handle the first row of each mac_address, 
            // because we care about the object (user) who has pressed 
            // the panic button within time interval and its latest lbeacon
            // location but not the lbeacon_uuid opon which the object (user)
            // pressed the button.
            if(0 != strncmp(prev_mac_address, 
                            mac_address, 
                            strlen(mac_address))){

                strncpy(prev_mac_address, mac_address, strlen(mac_address));

                SQL_get_object_monitor_type(db, mac_address, 
                                            &object_monitor_type);
                if(MONITOR_PANIC != 
                   (MONITOR_PANIC & (ObjectMonitorType)object_monitor_type)){

                    continue;
                }

                pqescape_mac_address = 
                    PQescapeLiteral(conn, mac_address, 
                                    strlen(mac_address));
                pqescape_final_timestamp = 
                    PQescapeLiteral(conn, final_timestamp, 
                                    strlen(final_timestamp));
                     
                memset(sql, 0, sizeof(sql));
                sprintf(sql, sql_update_panic_template,  
                             pqescape_final_timestamp,
                             pqescape_mac_address);

                SQL_execute(db, sql);

                PQfreemem(pqescape_mac_address);
                PQfreemem(pqescape_final_timestamp);

                if(WORK_SUCCESSFULLY != ret_val){
                    
                    SQL_rollback_transaction(db);
                    PQclear(res);

                    zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                               ret_val, PQerrorMessage(conn));

                    return E_SQL_EXECUTE;
                }

            }
        }

        SQL_commit_transaction(db);
    }

    PQclear(res);
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_geo_fence(void *db, int time_interval_in_sec){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "SELECT mac_address, uuid, " \
                                "ROUND( AVG(rssi), 2) as avg_rssi, " \
                                "name, " \
                                "type, " \
                                "MAX(alert_time) as violation_timestamp " \
                                "FROM geo_fence_alert " \
                                "WHERE " \
                                "receive_time >= " \
                                "NOW() - INTERVAL '%d seconds' " \
                                "GROUP BY " \
                                "mac_address, uuid, name, type " \
                                "ORDER BY " \
                                "mac_address ASC, type ASC, avg_rssi DESC";
    PGresult *res = NULL;
    int current_row = 0;
    int total_fields = 0;
    int total_rows = 0;

    char *mac_address = NULL;
    char prev_mac_address[LENGTH_OF_MAC_ADDRESS];

    char *lbeacon_uuid = NULL;
    char *avg_rssi = NULL;
    char *name = NULL;
    char *type = NULL;
    char *violation_timestamp = NULL;

    char *pqescape_mac_address = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_name = NULL;
    char *pqescape_type = NULL;
    char *pqescape_violation_timestamp = NULL;

    char *sql_select_mac_address_lbeacon_uuid_template = 
        "SELECT mac_address, uuid " \
        "from " \
        "object_summary_table where " \
        "mac_address = %s AND uuid = %s";

    PGresult *res_mac_address = NULL;
    int rows_mac_address = 0;
    int fields_mac_address = 0;

    char *sql_update_location_geo_fence_template = 
        "UPDATE object_summary_table " \
        "set uuid = %s, " \
        "rssi = %d, " \
        "first_seen_timestamp = %s, " \
        "last_seen_timestamp = %s," \
        "geofence_name = %s, " \
        "geofence_type = %s, " \
        "geofence_violation_timestamp = %s " \
        "WHERE mac_address = %s";

    char *sql_update_geo_fence_template = 
        "UPDATE object_summary_table " \
        "set rssi = %d, " \
        "geofence_name = %s, " \
        "geofence_type = %s, " \
        "geofence_violation_timestamp = %s " \
        "WHERE mac_address = %s";

    ObjectMonitorType object_monitor_type = MONITOR_NORMAL;


    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template, 
            time_interval_in_sec);


    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    if(total_rows > 0 && total_fields == 6){

        memset(prev_mac_address, 0, sizeof(prev_mac_address));

        SQL_begin_transaction(db);

        for(current_row = 0 ; current_row < total_rows ; current_row++){

            mac_address = PQgetvalue(res, current_row, 0);

            // we only need to handle the first row of each pair of 
            // mac_address and lbeacon_uuid, because we have sorted 
            // the result by avg_rssi in the SQL command.
            if(0 != strncmp(prev_mac_address, 
                            mac_address, 
                            strlen(mac_address))){

                strncpy(prev_mac_address, mac_address, strlen(mac_address));

                SQL_get_object_monitor_type(db, mac_address, 
                                            &object_monitor_type);
                if(MONITOR_GEO_FENCE != 
                   (MONITOR_GEO_FENCE & (ObjectMonitorType)object_monitor_type)){
                
                    continue;
                }

                lbeacon_uuid = PQgetvalue(res, current_row, 1);
                avg_rssi = PQgetvalue(res, current_row, 2);
                name = PQgetvalue(res, current_row, 3);
                type = PQgetvalue(res, current_row, 4);
                violation_timestamp = PQgetvalue(res, current_row, 5);

                // first, check if the pair of mac_address and lbeacon_uuid 
                // exists in the object_summary_table.
                pqescape_mac_address = 
                    PQescapeLiteral(conn, mac_address, strlen(mac_address));
                pqescape_lbeacon_uuid = 
                        PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));

                sprintf(sql, sql_select_mac_address_lbeacon_uuid_template, 
                        pqescape_mac_address,
                        pqescape_lbeacon_uuid);

                res_mac_address = PQexec(conn, sql);

                PQfreemem(pqescape_mac_address);
                PQfreemem(pqescape_lbeacon_uuid);

                if(PQresultStatus(res_mac_address) != PGRES_TUPLES_OK){
                    PQclear(res_mac_address);
                    SQL_rollback_transaction(db);
                    PQclear(res);
                    
                    zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                               res_mac_address, PQerrorMessage(conn));

                    return E_SQL_EXECUTE;
                }

                rows_mac_address = PQntuples(res_mac_address);
                fields_mac_address = PQnfields(res_mac_address);

                if(rows_mac_address == 0){
                    // if the pair of mac_address and lbeacon_uuid does not exist
                    // in the object_summary_table, we update the whole location 
                    // information with geofence alert 

                    pqescape_mac_address = 
                        PQescapeLiteral(conn, mac_address, 
                                        strlen(mac_address));
                    pqescape_lbeacon_uuid = 
                        PQescapeLiteral(conn, lbeacon_uuid, 
                                        strlen(lbeacon_uuid));
                    pqescape_name =
                        PQescapeLiteral(conn, name,
                                        strlen(name));
                    pqescape_type =
                        PQescapeLiteral(conn, type,
                                        strlen(type));
                    pqescape_violation_timestamp = 
                        PQescapeLiteral(conn, violation_timestamp,
                                        strlen(violation_timestamp));

                    memset(sql, 0, sizeof(sql));
                    sprintf(sql, sql_update_location_geo_fence_template,  
                                 pqescape_lbeacon_uuid,
                                 atoi(avg_rssi), 
                                 pqescape_violation_timestamp, 
                                 pqescape_violation_timestamp,
                                 pqescape_name,
                                 pqescape_type,
                                 pqescape_violation_timestamp,
                                 pqescape_mac_address);

                    SQL_execute(db, sql);

                    PQfreemem(pqescape_mac_address);
                    PQfreemem(pqescape_lbeacon_uuid);
                    PQfreemem(pqescape_name);
                    PQfreemem(pqescape_type);
                    PQfreemem(pqescape_violation_timestamp);

                    if(WORK_SUCCESSFULLY != ret_val){
                        PQclear(res_mac_address);   
                        SQL_rollback_transaction(db);
                        PQclear(res);

                        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                                   ret_val, PQerrorMessage(conn));

                        return E_SQL_EXECUTE;
                    }

                    
                }
                else if(rows_mac_address == 1)
                {
                    if(fields_mac_address == 2){
                    // if the pair of mac_address and lbeacon_uuid exists
                    // in the object_summary_table, we update the geofence 
                    // violation.
                      
                        pqescape_mac_address = 
                            PQescapeLiteral(conn, mac_address, 
                                            strlen(mac_address));
                        pqescape_name = 
                            PQescapeLiteral(conn, name,
                                            strlen(name));
                        pqescape_type = 
                            PQescapeLiteral(conn, type,
                                            strlen(type));
                        pqescape_violation_timestamp = 
                            PQescapeLiteral(conn, violation_timestamp,
                                            strlen(violation_timestamp));

                     
                        memset(sql, 0, sizeof(sql));
                        sprintf(sql, sql_update_geo_fence_template,  
                                atoi(avg_rssi), 
                                pqescape_name,
                                pqescape_type,
                                pqescape_violation_timestamp,
                                pqescape_mac_address);

                        SQL_execute(db, sql);

                        PQfreemem(pqescape_mac_address);
                        PQfreemem(pqescape_name);
                        PQfreemem(pqescape_type);
                        PQfreemem(pqescape_violation_timestamp);

                        if(WORK_SUCCESSFULLY != ret_val){
                            PQclear(res_mac_address);    
                            SQL_rollback_transaction(db);
                            PQclear(res);

                            zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                                       ret_val, PQerrorMessage(conn));

                            return E_SQL_EXECUTE;
                        }
                    }
                        
                }
                PQclear(res_mac_address);                
            }
        }

        SQL_commit_transaction(db);
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_identify_last_movement_status(void *db, 
                                            int time_interval_in_min, 
                                            int each_time_slot_in_min,
                                            unsigned int rssi_delta){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "SELECT mac_address, uuid " \
                                "FROM object_summary_table " \
                                "ORDER BY " \
                                "mac_address ASC";
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
    int fields_activity = 0;

    char *time_slot_activity = NULL;

    char *sql_update_activity_template = 
        "UPDATE object_summary_table " \
        "SET last_activity_timestamp = %s " \
        "WHERE mac_address = %s";

    char *pqescape_time_slot_activity = NULL;
    
    ObjectMonitorType object_monitor_type = MONITOR_NORMAL;
   

    memset(sql, 0, sizeof(sql));

    sprintf(sql, sql_select_template);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    if(total_rows > 0 && total_fields == 2){

        SQL_begin_transaction(db);

        for(current_row = 0 ; current_row < total_rows ; current_row++){
            mac_address = PQgetvalue(res, current_row, 0);

            SQL_get_object_monitor_type(db, mac_address, 
                                        &object_monitor_type);
            if(MONITOR_MOVEMENT != 
               (MONITOR_MOVEMENT & (ObjectMonitorType)object_monitor_type)){
                
                continue;
            }


            mac_address = PQgetvalue(res, current_row, 0);
            lbeacon_uuid = PQgetvalue(res, current_row, 1);

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
                SQL_rollback_transaction(db);
                PQclear(res);
                    
                zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                           res_activity, PQerrorMessage(conn));

                return E_SQL_EXECUTE;
            }

            rows_activity = PQntuples(res_activity);
            fields_activity = PQnfields(res_activity);
 
            if(rows_activity > 0){
                if(fields_activity == 3){

                    time_slot_activity = PQgetvalue(res_activity, 0, 0);

                    pqescape_mac_address = 
                        PQescapeLiteral(conn, mac_address, 
                                        strlen(mac_address));

                    pqescape_time_slot_activity = 
                        PQescapeLiteral(conn, time_slot_activity,
                                        strlen(time_slot_activity));
                
                    memset(sql, 0, sizeof(sql));
                    
                    sprintf(sql, sql_update_activity_template,  
                            pqescape_time_slot_activity,
                            pqescape_mac_address);
                            
                    SQL_execute(db, sql);

                    PQfreemem(pqescape_mac_address);
                    PQfreemem(pqescape_time_slot_activity);

                    if(WORK_SUCCESSFULLY != ret_val){
                        PQclear(res_activity);   
                        SQL_rollback_transaction(db);
                        PQclear(res);

                        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                                   ret_val, PQerrorMessage(conn));

                        return E_SQL_EXECUTE;
                    }     
                    PQclear(res_activity);

                    continue;
                }
            }
            PQclear(res_activity);
        }

        SQL_commit_transaction(db);
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
        "violation_timestamp " \
        ") " \
        "SELECT %d, " \
        "mac_address, " \
        "uuid, " \
        "%s " \
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
    char *panic_violation_timestamp = "panic_timestamp";
    char *violation_timestamp_name = NULL;

    if(MONITOR_GEO_FENCE == monitor_type){
        violation_timestamp_name = geofence_violation_timestamp;
    }else if(MONITOR_PANIC == monitor_type){
        violation_timestamp_name = panic_violation_timestamp;
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

    zlog_debug(category_debug, "SQL_collect_violation_events [%d] %s", 
                               monitor_type, sql);

    SQL_begin_transaction(db);
    SQL_execute(db, sql);
    if(WORK_SUCCESSFULLY != ret_val){
        SQL_rollback_transaction(db);
        
        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   ret_val, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }     
    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_get_object_monitor_type(void *db, 
                                      char *mac_address, 
                                      ObjectMonitorType *monitor_type){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "SELECT monitor_type " \
                                "FROM object_table " \
                                "WHERE " \
                                "mac_address = %s";
    PGresult *res = NULL;
    int total_fields = 0;
    int total_rows = 0;
   
    char *pqescape_mac_address = NULL;
    char *object_monitor_type = NULL;
   
   
    memset(sql, 0, sizeof(sql));

    pqescape_mac_address = 
        PQescapeLiteral(conn, mac_address, strlen(mac_address));

    sprintf(sql, sql_select_template, pqescape_mac_address);

    res = PQexec(conn, sql);

    PQfreemem(pqescape_mac_address);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    *monitor_type = MONITOR_NORMAL;
    if(total_rows == 1 && total_fields == 1){
        object_monitor_type = PQgetvalue(res, 0, 0);
        *monitor_type = (ObjectMonitorType)atoi(object_monitor_type);
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_geo_fence_config(void *db,
                                      char *unique_key,
                                      char *name,
                                      char *perimeters,
                                      char *fences,
                                      char *hour_start,
                                      char *hour_end){

    PGconn *conn = (PGconn *) db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "INSERT INTO geo_fence_config " \
                         "(unique_key, " \
                         "name, " \
                         "perimeters, " \
                         "fences, " \
                         "hour_start, " \
                         "hour_end) " \
                         "VALUES " \
                         "(%s, %s, %s, %s, %s, %s)" \
                         "ON CONFLICT (unique_key) " \
                         "DO UPDATE SET " \
                         "name = %s, " \
                         "perimeters = %s, " \
                         "fences = %s, " \
                         "hour_start = %s, " \
                         "hour_end = %s;";

    char *pqescape_unique_key = NULL;
    char *pqescape_name = NULL;
    char *pqescape_perimeters = NULL;
    char *pqescape_fences = NULL;


    pqescape_unique_key = 
        PQescapeLiteral(conn, unique_key, strlen(unique_key));

    pqescape_name = 
        PQescapeLiteral(conn, name, strlen(name));

    pqescape_perimeters = 
        PQescapeLiteral(conn, perimeters, strlen(perimeters));

    pqescape_fences = 
        PQescapeLiteral(conn, fences, strlen(fences));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template,
            pqescape_unique_key,
            pqescape_name,
            pqescape_perimeters,
            pqescape_fences,
            hour_start, 
            hour_end,
            pqescape_name,
            pqescape_perimeters,
            pqescape_fences,
            hour_start,
            hour_end);

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    PQfreemem(pqescape_unique_key);
    PQfreemem(pqescape_name);
    PQfreemem(pqescape_perimeters);
    PQfreemem(pqescape_fences);

    if(WORK_SUCCESSFULLY != ret_val){
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_get_geo_fence_config(void *db, 
                                   char *unique_key, 
                                   int *enable,
                                   int *hour_start,
                                   int *hour_end){

    PGconn *conn = (PGconn *)db;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_select_template = "SELECT enable, hour_start, hour_end " \
                                "FROM geo_fence_config " \
                                "WHERE " \
                                "unique_key = %s";
    PGresult *res = NULL;
    int total_fields = 0;
    int total_rows = 0;

    char *pqescape_unique_key = NULL;


    memset(sql, 0, sizeof(sql));

    pqescape_unique_key = 
        PQescapeLiteral(conn, unique_key, strlen(unique_key));
    
    sprintf(sql, sql_select_template, pqescape_unique_key);

    res = PQexec(conn, sql);

    PQfreemem(pqescape_unique_key);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

        zlog_error(category_debug, "SQL_execute failed [%d]: %s", 
                   res, PQerrorMessage(conn));

        return E_SQL_EXECUTE;
    }

    total_fields = PQnfields(res);
    total_rows = PQntuples(res);

    *enable = 0;
    *hour_start = 0;
    *hour_end = 0;
    if(total_rows == 1 && total_fields == 3){
        *enable = atoi(PQgetvalue(res, 0, 0));
        *hour_start = atoi(PQgetvalue(res, 0, 1));
        *hour_end = atoi(PQgetvalue(res, 0, 2));
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}