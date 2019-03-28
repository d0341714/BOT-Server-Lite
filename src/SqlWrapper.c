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
     delete data in the database and some APIs for the BeDIS system use including 
     updating device status and object tracking data maintainance and updates.

  Version:

     1.0, 20190308

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


static ErrorCode SQL_execute(void* db, char* sql_statement){

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

    /* Create SQL statement */
    sql = "BEGIN TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_commit_transaction(void* db){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "END TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}


static ErrorCode SQL_rollback_transaction(void* db){

    ErrorCode ret_val = WORK_SUCCESSFULLY;
    char *sql;

    /* Create SQL statement */
    sql = "ROLLBACK;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_open_database_connection(char* conninfo, void** db){

    *db = (PGconn*) PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(*db) != CONNECTION_OK)
    {

#ifdef debugging
        printf("Connection to database failed: %s", PQerrorMessage(*db));
#endif
    
        return E_SQL_OPEN_DATABASE;
    }

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_close_database_connection(void* db){
    PGconn *conn = (PGconn *) db;
    PQfinish(conn);

    return WORK_SUCCESSFULLY;
}


ErrorCode SQL_vacuum_database(void* db){
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


ErrorCode SQL_retain_data(void* db, int retention_hours){

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


ErrorCode SQL_update_gateway_registration_status(void* db,
                                                 char* buf,
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
    char *ip_address = NULL;
    HealthStatus health_status = S_NORMAL_STATUS;

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
        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template,
                PQescapeLiteral(conn, ip_address, strlen(ip_address)),
                health_status, health_status);

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


ErrorCode SQL_query_registered_gateways(void* db,
                                        HealthStatus health_status,
                                        char* output,
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


ErrorCode SQL_update_lbeacon_registration_status(void* db,
                                                 char* buf,
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
                         "health_status, " \
                         "gateway_ip_address, " \
                         "registered_timestamp, " \
                         "last_report_timestamp) " \
                         "VALUES " \
                         "(%s, \'%d\', %s, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
                         "NOW()) " \
                         "ON CONFLICT (uuid) " \
                         "DO UPDATE SET health_status = \'%d\', " \
                         "gateway_ip_address = %s, " \
                         "last_report_timestamp = NOW() ;";
    char *uuid = NULL;
    HealthStatus health_status = S_NORMAL_STATUS;
    char *gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;

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

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template,
                PQescapeLiteral(conn, uuid, strlen(uuid)),
                health_status,
                PQescapeLiteral(conn, gateway_ip, strlen(gateway_ip)),
                PQescapeLiteral(conn, registered_timestamp_GMT,
                                strlen(registered_timestamp_GMT)),
                health_status,
                PQescapeLiteral(conn, gateway_ip, strlen(gateway_ip)));

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


ErrorCode SQL_update_gateway_health_status(void* db,
                                           char* buf,
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
        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template,
                PQescapeLiteral(conn, health_status, strlen(health_status)),
                PQescapeLiteral(conn, ip_address, strlen(ip_address)));

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


ErrorCode SQL_update_lbeacon_health_status(void* db,
                                           char* buf,
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
 
    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    string_begin = temp_buf;
    string_end = strstr(string_begin, DELIMITER_SEMICOLON);
    *string_end = '\0';

    numbers = atoi(string_begin);

    if(numbers <= 0){
        return E_SQL_PARSE;
    }

    gateway_ip = string_end + 1;
    string_end = strstr(gateway_ip, DELIMITER_SEMICOLON);
    *string_end = '\0';

    SQL_begin_transaction(db);

    while( numbers-- ){
        uuid = string_end + 1;
        string_end = strstr(uuid, DELIMITER_SEMICOLON);
        *string_end = '\0';

        health_status = string_end + 1;
        string_end = strstr(health_status, DELIMITER_SEMICOLON);
        *string_end = '\0';

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template,
                PQescapeLiteral(conn, health_status, strlen(health_status)),
                PQescapeLiteral(conn, uuid, strlen(uuid)));

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


ErrorCode SQL_update_object_tracking_data(void* db,
                                          char* buf,
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
                         "initial_timestamp, " \
                         "final_timestamp) " \
                         "VALUES " \
                         "(%s, %s, %s, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval, " \
                         "TIMESTAMP \'epoch\' + %s * \'1 second\'::interval);";
    char *lbeacon_uuid = NULL;
    char *gateway_ip = NULL;
    char *object_type = NULL;
    char *object_number = NULL;
    int numbers = 0;
    char *object_mac_address = NULL;
    char *initial_timestamp_GMT = NULL;
    char *final_timestamp_GMT = NULL;
    char *rssi = NULL;

    memset(temp_buf, 0, sizeof(temp_buf));
    memcpy(temp_buf, buf, buf_len);

    lbeacon_uuid = temp_buf;
    string_end = strstr(lbeacon_uuid, DELIMITER_SEMICOLON);
    *string_end = '\0';

    gateway_ip = string_end + 1;
    string_end = strstr(gateway_ip, DELIMITER_SEMICOLON);
    *string_end = '\0';

    SQL_begin_transaction(db);

    while(num_types--){

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

            /* Create SQL statement */
            memset(sql, 0, sizeof(sql));
            sprintf(sql, sql_template,
                    PQescapeLiteral(conn, object_mac_address,
                                    strlen(object_mac_address)),
                    PQescapeLiteral(conn, lbeacon_uuid, strlen(lbeacon_uuid)),
                    PQescapeLiteral(conn, rssi, strlen(rssi)),
                    PQescapeLiteral(conn, initial_timestamp_GMT,
                                    strlen(initial_timestamp_GMT)),
                    PQescapeLiteral(conn, final_timestamp_GMT,
                                    strlen(final_timestamp_GMT)));
                   
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
