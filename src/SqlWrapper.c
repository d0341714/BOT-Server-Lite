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

     1.0, 20190617

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

     Chun Yu Lai   , chunyu1202@gmail.com
 */

#include "SqlWrapper.h"


static ErrorCode SQL_execute(void *db, char *sql_statement){

    PGconn *conn = (PGconn *) db;
    PGresult *res;

#ifdef debugging
    zlog_info(category_debug, "SQL command = [%s]", sql_statement);
#endif

    res = PQexec(conn, sql_statement);

    if(PQresultStatus(res) != PGRES_COMMAND_OK){

#ifdef debugging
        zlog_info(category_debug, "SQL_execute failed [%d]: %s", res, PQerrorMessage(conn));
#endif

        PQclear(res);
        return E_SQL_EXECUTE;
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_begin_transaction(void* db){
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    pthread_mutex_lock(&db_lock);

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

    pthread_mutex_unlock(&db_lock);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_rollback_transaction(void *db){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "ROLLBACK;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    pthread_mutex_unlock(&db_lock);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_open_database_connection(char *conninfo, void **db){

    *db = (PGconn*) PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(*db) != CONNECTION_OK)
    {

#ifdef debugging
        zlog_info(category_debug, "Connection to database failed: %s", PQerrorMessage(*db));
#endif

        return E_SQL_OPEN_DATABASE;
    }

    pthread_mutex_init(&db_lock, 0);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_close_database_connection(void *db){
    PGconn *conn = (PGconn *) db;
    PQfinish(conn);

    pthread_mutex_destroy(&db_lock);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_vacuum_database(void *db){
    PGconn *conn = (PGconn *) db;
    char *table_name[5] = {"tracking_table",
                           "lbeacon_table",
                           "gateway_table",
                           "object_table",
                           "geo_fence_alert"};
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql_template = "VACUUM %s;";
    int idx = 0;

    SQL_begin_transaction(db);

    for(idx = 0; idx< 5 ; idx++){

        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template, table_name[idx]);

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


ErrorCode SQL_retain_data(void *db, int retention_hours){

    PGconn *conn = (PGconn *) db;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
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

#ifdef debugging
        zlog_info(category_debug, "SQL command = [%s]", sql);
#endif

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
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
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

    string_begin = temp_buf;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    numbers = atoi(string_begin);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    SQL_begin_transaction(db);

    while( numbers-- ){

        ip_address = string_end + 1;
        string_end = strstr(ip_address, DELIMITER_SEMICOLON);
        *string_end = '\0';

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


ErrorCode SQL_query_registered_gateways(void *db,
                                        HealthStatus health_status,
                                        char *output,
                                        size_t output_len){

    PGconn *conn = (PGconn *) db;
    PGresult *res;
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char sql[SQL_TEMP_BUFFER_LENGTH];

    char *sql_template_query_all =
                         "SELECT ip_address, health_status FROM " \
                         "gateway_table;" ;

    char *sql_template_query_health_status =
                         "SELECT ip_address, health_status FROM " \
                         "gateway_table WHERE " \
                         "health_status = \'%d\';" ;
    int rows = 0;
    int i = 0;
    char temp_buf[SQL_TEMP_BUFFER_LENGTH];

    /* Create SQL statement */
    memset(sql, 0, sizeof(sql));

    if(MAX_STATUS == health_status ){
        sprintf(sql, sql_template_query_all);
    }else{
        sprintf(sql, sql_template_query_health_status, health_status);
    }

    /* Execute SQL statement */

    SQL_begin_transaction(db);

#ifdef debugging
    zlog_info(category_debug, "SQL command = [%s]", sql);
#endif

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

#ifdef debugging
        zlog_info(category_debug, "SQL_execute failed: %s", PQerrorMessage(conn));
#endif

        SQL_rollback_transaction(db);

        return E_SQL_EXECUTE;
    }

    rows = PQntuples(res);

    sprintf(output, "%d;", rows);

    for(i = 0 ; i < rows ; i ++){

        memset(temp_buf, 0, sizeof(temp_buf));
        sprintf(temp_buf, "%s;%s;", PQgetvalue(res,i,0),PQgetvalue(res,i,1));

        if(output_len < strlen(output) + strlen(temp_buf)){

#ifdef debugging
            zlog_info(category_debug,"String concatenation failed due to output buffer size");
#endif

            PQclear(res);

            SQL_rollback_transaction(db);

            return E_SQL_RESULT_EXCEED;
        }

        strcat(output, temp_buf);

    }

    PQclear(res);

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_lbeacon_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
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
    char *gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;
    char *pqescape_uuid = NULL;
    char *pqescape_lbeacon_ip = NULL;
    char *pqescape_gateway_ip = NULL;
    char *pqescape_registered_timestamp_GMT = NULL;

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    string_begin = temp_buf;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    numbers = atoi(string_begin);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    string_begin = string_end + 1;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';
    gateway_ip = string_begin;

    SQL_begin_transaction(db);

    while( numbers-- ){

        uuid = string_end + 1;
        string_end = strstr(uuid, DELIMITER_SEMICOLON);
        *string_end = '\0';

        registered_timestamp_GMT = string_end + 1;
        string_end = strstr(registered_timestamp_GMT, DELIMITER_SEMICOLON);
        *string_end = '\0';

        lbeacon_ip = string_end + 1;
        string_end = strstr(lbeacon_ip, DELIMITER_SEMICOLON);
        *string_end = '\0';

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));

        pqescape_uuid = PQescapeLiteral(conn, uuid, strlen(uuid));
        pqescape_lbeacon_ip =
            PQescapeLiteral(conn, lbeacon_ip, strlen(lbeacon_ip));
        pqescape_gateway_ip =
            PQescapeLiteral(conn, gateway_ip, strlen(gateway_ip));
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
                                           size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql_template = "UPDATE gateway_table " \
                         "SET health_status = %s, " \
                         "last_report_timestamp = NOW() " \
                         "WHERE ip_address = %s ;" ;
    char *ip_address = NULL;
    char *health_status = NULL;
    char *pqescape_ip_address = NULL;
    char *pqescape_health_status = NULL;

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    ip_address = temp_buf;
    string_end = strstr(ip_address, DELIMITER_SEMICOLON);
    *string_end = '\0';

    health_status = string_end + 1;
    string_end = strstr(health_status, DELIMITER_SEMICOLON);
    *string_end = '\0';

    SQL_begin_transaction(db);

    /* Create SQL statement */
    pqescape_ip_address =
        PQescapeLiteral(conn, ip_address, strlen(ip_address));
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
                                           size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql_template = "UPDATE lbeacon_table " \
                         "SET health_status = %s, " \
                         "last_report_timestamp = NOW() " \
                         "WHERE uuid = %s ;";
    char *lbeacon_uuid = NULL;
    char *lbeacon_timestamp = NULL;
    char *lbeacon_ip = NULL;
    char *health_status = NULL;
    char *pqescape_lbeacon_uuid = NULL;
    char *pqescape_health_status = NULL;

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = temp_buf;
    string_end = strstr(lbeacon_uuid, DELIMITER_SEMICOLON);
    *string_end = '\0';

    lbeacon_timestamp = string_end + 1;
    string_end = strstr(lbeacon_timestamp, DELIMITER_SEMICOLON);
    *string_end = '\0';

    lbeacon_ip = string_end + 1;
    string_end = strstr(lbeacon_ip, DELIMITER_SEMICOLON);
    *string_end = '\0';

    health_status = string_end + 1;
    string_end = strstr(health_status, DELIMITER_SEMICOLON);
    *string_end = '\0';

    SQL_begin_transaction(db);

    /* Create SQL statement */
    pqescape_lbeacon_uuid = PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid));
    pqescape_health_status =
        PQescapeLiteral(conn, health_status, strlen(health_status));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template,
            pqescape_health_status,
            pqescape_lbeacon_uuid);

    PQfreemem(pqescape_lbeacon_uuid);
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


ErrorCode SQL_update_object_tracking_data(void *db,
                                          char *buf,
                                          size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_end;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
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

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = temp_buf;
    string_end = strstr(lbeacon_uuid, DELIMITER_SEMICOLON);
    *string_end = '\0';

    lbeacon_timestamp = string_end + 1;
    string_end = strstr(lbeacon_timestamp, DELIMITER_SEMICOLON);
    *string_end = '\0';

    lbeacon_ip = string_end + 1;
    string_end = strstr(lbeacon_ip, DELIMITER_SEMICOLON);
    *string_end = '\0';

    SQL_begin_transaction(db);

    while(num_types --){

        object_type = string_end + 1;
        string_end = strstr(object_type, DELIMITER_SEMICOLON);
        *string_end = '\0';

        object_number = string_end + 1;
        string_end = strstr(object_number, DELIMITER_SEMICOLON);
        *string_end = '\0';

        numbers = atoi(object_number);

        while(numbers--){
            object_mac_address = string_end + 1;
            string_end = strstr(object_mac_address, DELIMITER_SEMICOLON);
            *string_end = '\0';

            initial_timestamp_GMT = string_end + 1;
            string_end = strstr(initial_timestamp_GMT, DELIMITER_SEMICOLON);
            *string_end = '\0';

            final_timestamp_GMT = string_end + 1;
            string_end = strstr(final_timestamp_GMT, DELIMITER_SEMICOLON);
            *string_end = '\0';

            rssi = string_end + 1;
            string_end = strstr(rssi, DELIMITER_SEMICOLON);
            *string_end = '\0';

            push_button = string_end + 1;
            string_end = strstr(push_button, DELIMITER_SEMICOLON);
            *string_end = '\0';

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


ErrorCode SQL_insert_geo_fence_alert(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char *saved_ptr;
    char content_temp[WIFI_MESSAGE_LENGTH];
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql_template = "INSERT INTO geo_fence_alert " \
                         "(mac_address, " \
                         "type, " \
                         "uuid, " \
                         "alert_time, " \
                         "rssi, " \
                         "receive_time) " \
                         "VALUES " \
                         "(%s, %s, %s, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
                         "%s, NOW());";

    int number_of_geo_fence_alert = 0;
    char *number_of_geo_fence_alert_str = NULL;
    char *mac_address = NULL;
    char *type = NULL;
    char *uuid = NULL;
    char *alert_time = NULL;
    char *rssi = NULL;

    char *pqescape_mac_address = NULL;
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
        type = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        uuid = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        alert_time = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);
        rssi = strtok_save(NULL, DELIMITER_SEMICOLON, &saved_ptr);

        /* Create SQL statement */
        memset(sql, 0, strlen(sql) * sizeof(char));

        pqescape_mac_address = PQescapeLiteral(conn, mac_address,
                                               strlen(mac_address));
        pqescape_type = PQescapeLiteral(conn, type, strlen(type));
        pqescape_uuid = PQescapeLiteral(conn, uuid, strlen(uuid));
        pqescape_alert_time = PQescapeLiteral(conn, alert_time,
                                              strlen(alert_time));
        pqescape_rssi = PQescapeLiteral(conn, rssi, strlen(rssi));


        sprintf(sql, sql_template,
                pqescape_mac_address,
                pqescape_type,
                pqescape_uuid,
                pqescape_alert_time,
                pqescape_rssi);

        PQfreemem(pqescape_mac_address);
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