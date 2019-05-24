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

     1.0, 20190519

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
    printf("SQL command = [%s]\n", sql_statement);
#endif

    res = PQexec(conn, sql_statement);

    if(PQresultStatus(res) != PGRES_COMMAND_OK){

#ifdef debugging
        printf("SQL_execute failed [%d]: %s", res, PQerrorMessage(conn));
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
        printf("Connection to database failed: %s", PQerrorMessage(*db));
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
    char *table_name[4] = {"tracking_table",
                           "lbeacon_table",
                           "gateway_table",
                           "object_table"};
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql_template = "VACUUM %s;";
    int idx = 0;

    for(idx = 0; idx< 4 ; idx++){

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
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *table_name[3] = {"lbeacon_table", "gateway_table", "tracking_table"};
    char *sql_template = "DELETE FROM %s WHERE " \
                         "last_report_timestamp < " \
                         "NOW() - INTERVAL \'%d HOURS\';";
    int idx = 0;
    char *tsdb_table_name[1] = {"tracking_table"};
    char *sql_tsdb_template = "SELECT drop_chunks(interval \'%d HOURS\', " \
                              "\'%s\');";
    PGresult *res;

    for(idx = 0; idx<3; idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_template, table_name[idx], retention_hours);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            return E_SQL_EXECUTE;
        }

    }

    for(idx = 0; idx<1; idx++){

        memset(sql, 0, sizeof(sql));

        sprintf(sql, sql_tsdb_template, retention_hours, tsdb_table_name[idx]);

        /* Execute SQL statement */

#ifdef debugging
        printf("SQL command = [%s]\n", sql);
#endif

        res = PQexec(conn, sql);
        if(PQresultStatus(res) != PGRES_TUPLES_OK){

            PQclear(res);
            printf("SQL_execute failed: %s", PQerrorMessage(conn));
            return E_SQL_EXECUTE;

        }
        PQclear(res);
    }

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

#ifdef debugging
    printf("SQL command = [%s]\n", sql);
#endif

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif

        return E_SQL_EXECUTE;
    }

    rows = PQntuples(res);

    sprintf(output, "%d;", rows);

    for(i = 0 ; i < rows ; i ++){

        memset(temp_buf, 0, sizeof(temp_buf));
        sprintf(temp_buf, "%s;%s;", PQgetvalue(res,i,0),PQgetvalue(res,i,1));

        if(output_len < strlen(output) + strlen(temp_buf)){

#ifdef debugging
            printf("String concatenation failed due to output buffer size\n");
#endif

            PQclear(res);
            return E_SQL_RESULT_EXCEED;
        }

        strcat(output, temp_buf);

    }

    PQclear(res);

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
    char *string_begin;
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

        health_status = string_end + 1;
        string_end = strstr(health_status, DELIMITER_SEMICOLON);
        *string_end = '\0';

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

    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_update_lbeacon_health_status(void *db,
                                           char *buf,
                                           size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *gateway_ip = NULL;
    char *sql_template = "UPDATE lbeacon_table " \
                         "SET health_status = %s, " \
                         "last_report_timestamp = NOW() " \
                         "WHERE uuid = %s ;";
    char *uuid = NULL;
    char *health_status = NULL;
    char *pqescape_uuid = NULL;
    char *pqescape_health_status = NULL;

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
        uuid = string_end + 1;
        string_end = strstr(uuid, DELIMITER_SEMICOLON);
        *string_end = '\0';

        gateway_ip = string_end + 1;
        string_end = strstr(gateway_ip, DELIMITER_SEMICOLON);
        *string_end = '\0';

        health_status = string_end + 1;
        string_end = strstr(health_status, DELIMITER_SEMICOLON);
        *string_end = '\0';

        /* Create SQL statement */
        pqescape_uuid = PQescapeLiteral(conn, uuid, strlen(uuid));
        pqescape_health_status = 
            PQescapeLiteral(conn, health_status, strlen(health_status));

        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template,
                pqescape_health_status,
                pqescape_uuid);

        PQfreemem(pqescape_uuid);
        PQfreemem(pqescape_health_status);

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
                         "push_button, " \
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


int SQL_update_api_data_owner(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin, *string_end;
    char *topic_name = NULL;
    char *ip_address = NULL;
    char *pqescape_topic_name = NULL;
    char *pqescape_ip_address = NULL;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    PGresult *res;
    int topic_id;
    char *sql_insert_template = "INSERT INTO api_data_owner " \
                                "(name, " \
                                "ip_address) " \
                                "VALUES " \
                                "(%s, %s) " \
                                "ON CONFLICT (name) " \
                                "DO UPDATE SET ip_address = %s;";

    char *sql_select_template = "SELECT id, name, ip_address FROM " \
                                "api_data_owner " \
                                "where name = %s;";

    memset(temp_buf, 0, WIFI_MESSAGE_LENGTH);
    memcpy(temp_buf, buf, buf_len);

    string_begin = temp_buf;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    topic_name = string_begin;

    string_begin = string_end + 1;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    ip_address = string_begin;

    SQL_begin_transaction(db);

    /* Create SQL statement */
    pqescape_topic_name = 
        PQescapeLiteral(conn, topic_name, strlen(topic_name));
    pqescape_ip_address = 
        PQescapeLiteral(conn, ip_address, strlen(ip_address));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_insert_template,
            pqescape_topic_name,
            pqescape_ip_address,
            pqescape_ip_address);

    PQfreemem(pqescape_topic_name);
    PQfreemem(pqescape_ip_address);

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        SQL_rollback_transaction(db);
#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif
        return -1;
    }

    SQL_commit_transaction(db);

    pqescape_topic_name = 
        PQescapeLiteral(conn, topic_name, strlen(topic_name));
    
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_select_template,
                 pqescape_topic_name);

    PQfreemem(pqescape_topic_name);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif
        return -1;

    }

    sscanf(PQgetvalue(res, 0, 0), "%d", &topic_id);

    PQclear(res);

    return topic_id;
}


ErrorCode SQL_remove_api_data_owner(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    PGresult *res;
    int topic_id;
    char *sql_delete_template = "DELETE FROM api_data_owner where id =\'%d\';";

    sscanf(buf, "%d;", &topic_id);
   
    SQL_begin_transaction(db);

    /* Create SQL statement */
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_delete_template, topic_id);

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        SQL_rollback_transaction(db);
#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif
        return E_SQL_EXECUTE;
    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;

}


int SQL_get_api_data_owner_id(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char temp_buf[WIFI_MESSAGE_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    PGresult *res;
    int topic_id, rows;
    char *topic_name, *string_begin, *string_end;
    char *pqescape_topic_name = NULL;

    char *sql_select_template = "SELECT id, name, ip_address FROM "\
                                "api_data_owner where name = %s ;";

    memset(temp_buf, 0, WIFI_MESSAGE_LENGTH); 
    memcpy(temp_buf, buf, buf_len);

    string_begin = temp_buf;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    topic_name = string_begin;

    pqescape_topic_name = 
        PQescapeLiteral(conn, topic_name, strlen(topic_name));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_select_template,
                 pqescape_topic_name);

    PQfreemem(pqescape_topic_name);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif
        return -1;

    }

    rows = PQntuples(res);

    if(rows == 0)
        topic_id = -1;
    else
        sscanf(PQgetvalue(res, 0, 0), "%d", &topic_id);

    PQclear(res);

    return topic_id;

}


int SQL_update_api_subscription(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin, *string_end;
    char *ip_address, *topic_name;
    char *pqescape_ip_addreee = NULL;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    PGresult *res;
    int topic_id, subscriber_id, rows;
    char *sql_insert_template = "INSERT INTO api_subscriber " \
                                "(topic_id, " \
                                "ip_address) " \
                                "VALUES " \
                                "(\'%d\', %s) ;";

    char *sql_subscription_template = "SELECT id, topic_id, ip_address FROM "\
                                      "api_subscriber where topic_id = \'%d\' "\
                                      "and ip_address = %s ;";

	topic_id = SQL_get_api_data_owner_id(db, buf, buf_len);

	if(topic_id < 0)
	{
	    return -1;
	}

    memset(temp_buf, 0, WIFI_MESSAGE_LENGTH);
    memcpy(temp_buf, buf, buf_len);

    string_begin = temp_buf;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    topic_name = string_begin;
    
    string_begin = string_end + 1;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    ip_address = string_begin;

    pqescape_ip_addreee = 
        PQescapeLiteral(conn, ip_address, strlen(ip_address));

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_subscription_template,
                 topic_id,
                 pqescape_ip_addreee);

    PQfreemem(pqescape_ip_addreee);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){

#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif

        PQclear(res);
        return -1;
    }

    if(PQntuples(res) == 0){

		SQL_begin_transaction(db);

        /* Create SQL statement */
        pqescape_ip_addreee = 
            PQescapeLiteral(conn, ip_address, strlen(ip_address));

        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_insert_template, topic_id,
                pqescape_ip_addreee);

        PQfreemem(pqescape_ip_addreee);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);

        if(WORK_SUCCESSFULLY != ret_val){
            SQL_rollback_transaction(db);

#ifdef debugging
            printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif

            return -1;
        }

        SQL_commit_transaction(db);

        pqescape_ip_addreee = 
            PQescapeLiteral(conn, ip_address, strlen(ip_address));

        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_subscription_template,
                     topic_id,
                     pqescape_ip_addreee);

        PQfreemem(pqescape_ip_addreee);

        res = PQexec(conn, sql);
    }

    sscanf(PQgetvalue(res, 0, 0), "%d", &subscriber_id);

    PQclear(res);

    return subscriber_id;

}


ErrorCode SQL_remove_api_subscription(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    ErrorCode ret_val = WORK_SUCCESSFULLY;
    PGresult *res;
    int subscriber_id;
    char *sql_delete_template = "DELETE FROM api_subscriber where id =\'%d\';";

    sscanf(buf, "%d;", &subscriber_id);

    memset(sql, 0, SQL_TEMP_BUFFER_LENGTH);

    SQL_begin_transaction(db);

    /* Create SQL statement */
    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_delete_template, subscriber_id);

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    if(WORK_SUCCESSFULLY != ret_val){
        SQL_rollback_transaction(db);

#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif

        return E_SQL_EXECUTE;
    }

    SQL_commit_transaction(db);

    return WORK_SUCCESSFULLY;

}


ErrorCode SQL_get_api_subscribers(void *db, char *buf, size_t buf_len){

    PGconn *conn = (PGconn *) db;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    PGresult *res;
    int topic_id, rows, current_row;

    char *sql_template = "SELECT id, topic_id, ip_address FROM "\
                         "api_subscriber where topic_id = \'%d\' ;";

    topic_id = SQL_get_api_data_owner_id(db, buf, buf_len);

	if(topic_id < 0)
	{
	    return E_SQL_EXECUTE;
	}

    memset(sql, 0, sizeof(sql));
    sprintf(sql, sql_template, topic_id);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);

#ifdef debugging
        printf("SQL_execute failed: %s", PQerrorMessage(conn));
#endif

        return E_SQL_EXECUTE;

    }

    rows = PQntuples(res);

    if(rows > 0){
        for(current_row=0;current_row < rows;current_row ++){
            if(strlen(buf) > 0)
                sprintf(buf, "%s;%s;", buf, PQgetvalue(res, current_row, 2));
            else
                sprintf(buf, "%s;", PQgetvalue(res, current_row, 2));
        }
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;

}


ErrorCode SQL_get_geo_fence(void *db, char *buf){

    PGconn *conn = (PGconn *) db;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    PGresult *res;
    int rows, current_row;

    char *sql_select_template = "SELECT id, name, perimeter, fence, " \
                                "mac_prefix FROM geo_fence;";

    memset(sql, 0, SQL_TEMP_BUFFER_LENGTH);

    sprintf(sql, sql_select_template);

    res = PQexec(conn, sql);

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
#ifdef debugging
        printf("SQL_execute failed: %s\n", PQerrorMessage(conn));
#endif
        sprintf(buf, "0;");
        return E_SQL_EXECUTE;

    }

    rows = PQntuples(res);

    if(rows == 0){
        sprintf(buf, "0;");
    }else{
        for(current_row=0;current_row < rows;current_row++){
            if(strlen(buf) > 0)
                sprintf(buf, "%s;%s;%s;%s;%s;%s;",
                                                buf,
                                                PQgetvalue(res, current_row, 0),
                                                PQgetvalue(res, current_row, 1),
                                                PQgetvalue(res, current_row, 2),
                                                PQgetvalue(res, current_row, 3),
                                                PQgetvalue(res, current_row, 4));
            else
                sprintf(buf, "%d;%s;%s;%s;%s;%s;",
                                                rows,
                                                PQgetvalue(res, current_row, 0),
                                                PQgetvalue(res, current_row, 1),
                                                PQgetvalue(res, current_row, 2),
                                                PQgetvalue(res, current_row, 3),
                                                PQgetvalue(res, current_row, 4));
	    }
    }

    PQclear(res);

    return WORK_SUCCESSFULLY;

}
