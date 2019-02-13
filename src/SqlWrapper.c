#include "SqlWrapper.h"

static int SQL_callback(void *NotUsed,
                        int argc,
                        char **argv,
                        char **azColName){
    int i;
    for(i = 0; i < argc;i ++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

static ErrorCode SQL_execute(void* db, char* sql_statement){
    sqlite3 *sql_db = (sqlite3*) db;
    char *zErrMsg = NULL;
    int ret_val;

    /* Execute SQL statement */
    printf("SQL command=[%s]\n", sql_statement);
    ret_val = sqlite3_exec(sql_db, sql_statement, SQL_callback, 0, &zErrMsg);
    if( ret_val != SQLITE_OK ){
        fprintf(stderr, "SQL error: [%d] [%s]\n", ret_val, zErrMsg);
        sqlite3_free(zErrMsg);

        return E_SQL_EXECUTE;
    }else{
        fprintf(stdout, "Records created successfully\n");
    }

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_begin_transaction(void* db){
    int ret_val;
    char *sql;

    /* Create SQL statement */
    sql = "BEGIN TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_end_transaction(void* db){
    int ret_val = 0;
    char *sql;

    /* Create SQL statement */
    sql = "END TRANSACTION;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}

static ErrorCode SQL_rollback_transaction(void* db){
    int ret_val = 0;
    char *sql;

    /* Create SQL statement */
    sql = "ROLLBACK;";

    /* Execute SQL statement */
    ret_val = SQL_execute(db, sql);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_open_database_connection(char* db_filepath, void** db){
    sqlite3 **sql_db = (sqlite3**)db;
    int ret_val = sqlite3_open(db_filepath, sql_db);

    if(ret_val){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*sql_db));
        return E_SQL_OPEN_DATABASE;
    }else{
        fprintf(stderr, "Opened database successfully\n");
    }
    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_close_database_connection(void* db){
    sqlite3 *sql_db = (sqlite3*)db;
    sqlite3_close(sql_db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_gateway_registration_status(void* db,
                                                 char* buf,
                                                 size_t buf_len){
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "INSERT OR REPLACE INTO gateway_table " \
                         "(ip_address, " \
                         "health_status, " \
                         "registered_timestamp_GMT, " \
                         "last_report_timestamp_GMT) " \
                         "VALUES " \
                         "(\"%s\", \"%d\", \"%d\", \"%d\")";
    char *ip_address = NULL;
    int health_status = 0;
    //char *descritpion = "Sample description";
    int current_timestamp = get_system_time();

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
        sprintf(sql, sql_template, ip_address, health_status,
                current_timestamp, current_timestamp);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);
        if(0 != ret_val){
            break;
        }
    }

    if(0 == ret_val){
        SQL_end_transaction(db);
    }else{
        SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_lbeacon_registration_status(void* db,
                                                 char* buf,
                                                 size_t buf_len){
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "INSERT OR REPLACE INTO lbeacon_table " \
                         "(uuid, " \
                         "health_status, " \
                         "gateway_ip, " \
                         "registered_timestamp_GMT, " \
                         "last_report_timestamp_GMT) " \
                         "VALUES " \
                         "(\"%s\", \"%d\", \"%s\", \"%s\", \"%d\")";
    char *uuid = NULL;
    int health_status = 0;
    char *gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;
    int current_timestamp = get_system_time();

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
        sprintf(sql, sql_template, uuid, health_status, gateway_ip,
                registered_timestamp_GMT, current_timestamp);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);
        if(0 != ret_val){
            break;
        }
    }

    if(0 == ret_val){
        SQL_end_transaction(db);
    }else{
        SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_query_registered_gateways(void* db,
                                        int health_status,
                                        char* output,
                                        size_t output_len){
    sqlite3 *sql_db = (sqlite3*)db;
    int ret_val = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    char *sql_template = "SELECT ip_address, health_status FROM " \
                         "`gateway_table` WHERE " \
                         "health_status = \"%d\" OR health_status = \"%d\";";
    sqlite3_stmt *stmt;
    int count = 0;
    char temp_buf[SQL_TEMP_BUFFER_LENGTH];
    char result_buf[SQL_TEMP_BUFFER_LENGTH];

    /* Create SQL statement */
    memset(sql, 0, sizeof(sql));
    if(-1 == health_status ){
        sprintf(sql, sql_template, 0, 1);
    }else{
        sprintf(sql, sql_template, health_status, health_status);
    }

    /* Execute SQL statement */
    sqlite3_prepare_v2(sql_db, sql, strlen(sql), &stmt, NULL);

    memset(result_buf, 0, sizeof(result_buf));
    while((ret_val = sqlite3_step(stmt)) == SQLITE_ROW){
        count++;

        /* Ensure the SQL query result does not exceed the buffer
        */
        if(sizeof(result_buf) < strlen(result_buf) + (NETWORK_ADDR_LENGTH + 3)){
            return E_SQL_RESULT_EXCEED;
        }

        memset(temp_buf, 0, sizeof(temp_buf));
        sprintf(temp_buf, "%s;%d;", sqlite3_column_text(stmt, 0),
                sqlite3_column_int(stmt, 1));

        strcat(result_buf, temp_buf);
    }

    sqlite3_finalize(stmt);

    if(output_len < strlen(result_buf)){
        return E_SQL_RESULT_EXCEED;
    }

    memset(output, 0, output_len);
    sprintf(output, "%d;%s", count, result_buf);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_gateway_health_status(void* db,
                                           char* buf,
                                           size_t buf_len){
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "INSERT OR REPLACE INTO gateway_table " \
                         "(ip_address, " \
                         "health_status, " \
                         "last_report_timestamp_GMT) " \
                         "VALUES " \
                         "(\"%s\", \"%s\", \"%d\")";
    char *ip_address = NULL;
    char *health_status = NULL;
    int current_timestamp = get_system_time();

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
        sprintf(sql, sql_template, ip_address, health_status,
                current_timestamp);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);
        if(0 != ret_val){
            break;
        }
    }

    if(0 == ret_val){
        SQL_end_transaction(db);
    }else{
        SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_lbeacon_health_status(void* db,
                                           char* buf,
                                           size_t buf_len){
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "INSERT OR REPLACE INTO lbeacon_table " \
                         "(uuid, " \
                         "health_status, " \
                         "last_report_timestamp_GMT) " \
                         "VALUES " \
                         "(\"%s\", \"%s\", \"%d\")";
    char *uuid = NULL;
    char *health_status = NULL;
    int current_timestamp = get_system_time();

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

        health_status = string_end + 1;
        string_end = strstr(health_status, DELIMITER_SEMICOLON);
        *string_end = '\0';

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template, uuid, health_status, current_timestamp);

        /* Execute SQL statement */
        ret_val = SQL_execute(db, sql);
        if(0 != ret_val){
            break;
        }
    }

    if(0 == ret_val){
        SQL_end_transaction(db);
    }else{
        SQL_rollback_transaction(db);
        return E_SQL_EXECUTE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_object_tracking_data(void* db,
                                          char* buf,
                                          size_t buf_len){
    SQL_begin_transaction(db);
    SQL_end_transaction(db);

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_update_object_geo_fencing_alert(void* db,
                                              char* buf,
                                              size_t buf_len){
    SQL_begin_transaction(db);
    SQL_end_transaction(db);

    return WORK_SUCCESSFULLY;
}
