#include "SqlWrapper.h"
#include "libpq-fe.h"

time_t get_system_time(){
    time_t ltime;
    struct tm* timeinfo;

	time(&ltime);
	timeinfo = gmtime(&ltime);
	ltime = mktime(timeinfo);
	return ltime;
}
/*
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
*/

static ErrorCode SQL_execute(void* db, char* sql_statement){
	PGconn *conn = (PGconn *) db;
	PGresult *res;

	printf("SQL command = [%s]\n", sql_statement);
	res = PQexec(conn, sql_statement);
	if(PQresultStatus(res) != PGRES_COMMAND_OK){
	    PQclear(res);
		printf("SQL_execute failed: %s", PQerrorMessage(conn));
		return E_SQL_EXECUTE;
	}

    PQclear(res);

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

ErrorCode SQL_open_database_connection(char* conninfo, void** db){
	*db = (PGconn*) PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(*db) != CONNECTION_OK)
    {
        printf("Connection to database failed: %s", PQerrorMessage(*db));
		return E_SQL_OPEN_DATABASE;
    }

    return WORK_SUCCESSFULLY;
}

ErrorCode SQL_close_database_connection(void* db){
	PGconn *conn = (PGconn *) db;
    PQfinish(conn);
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
    char *sql_template = "INSERT INTO gateway_table " \
                         "(ip_address, " \
                         "health_status, " \
                         "registered_timestamp, " \
                         "last_report_timestamp) " \
                         "VALUES " \
                         "(\'%s\', \'%d\', to_timestamp(\'%d\'), to_timestamp(\'%d\') ) " \
						 "ON CONFLICT (ip_address) " \
						 "DO UPDATE SET health_status = \'%d\', last_report_timestamp = to_timestamp(\'%d\') ;";
    char *ip_address = NULL;
    int health_status = 0;
    time_t current_timestamp = get_system_time();

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
                current_timestamp, current_timestamp, health_status, current_timestamp);

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

/*
ErrorCode SQL_query_registered_gateways(void* db,
                                        int health_status,
                                        char* output,
                                        size_t output_len){
											/*
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
											/*
    memset(sql, 0, sizeof(sql));
    if(-1 == health_status ){
        sprintf(sql, sql_template, 0, 1);
    }else{
        sprintf(sql, sql_template, health_status, health_status);
    }

    /* Execute SQL statement */
											/*
    sqlite3_prepare_v2(sql_db, sql, strlen(sql), &stmt, NULL);

    memset(result_buf, 0, sizeof(result_buf));
    while((ret_val = sqlite3_step(stmt)) == SQLITE_ROW){
        count++;

        /* Ensure the SQL query result does not exceed the buffer
        */
											/*
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
*/
/*
    return WORK_SUCCESSFULLY;
}
*/

ErrorCode SQL_update_lbeacon_registration_status(void* db,
                                                 char* buf,
                                                 size_t buf_len){
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "INSERT INTO lbeacon_table " \
                         "(uuid, " \
                         "health_status, " \
                         "gateway_ip_address, " \
                         "registered_timestamp, " \
                         "last_report_timestamp) " \
                         "VALUES " \
                         "(\'%s\', \'%d\', \'%s\', to_timestamp(\'%s\'), to_timestamp(\'%d\')) " \
						 "ON CONFLICT (uuid) " \
						 "DO UPDATE SET health_status = \'%d\', gateway_ip_address = \'%s\', last_report_timestamp = to_timestamp(\'%d\') ;";
    char *uuid = NULL;
    int health_status = 0;
    char *gateway_ip = NULL;
    char *registered_timestamp_GMT = NULL;
    time_t current_timestamp = get_system_time();

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
                registered_timestamp_GMT, current_timestamp, health_status, gateway_ip, current_timestamp);

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


ErrorCode SQL_update_gateway_health_status(void* db,
                                           char* buf,
                                           size_t buf_len){
    char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "UPDATE gateway_table " \
                         "SET health_status = \'%s'\, " \
						 "last_report_timestamp = to_timestamp(\'%d\') " \
                         "WHERE ip_address = \'%s\' ;" ;
    char *ip_address = NULL;
    char *health_status = NULL;
    time_t current_timestamp = get_system_time();

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
        sprintf(sql, sql_template, health_status, current_timestamp, ip_address);

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
    char *gateway_ip = NULL;
    char *sql_template = "UPDATE lbeacon_table " \
                         "SET health_status = \'%s\', " \
                         "last_report_timestamp = to_timestamp(\'%d\') " \
                         "WHERE uuid = \'%s\' ;";
    char *uuid = NULL;
    char *health_status = NULL;
    time_t current_timestamp = get_system_time();

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
        sprintf(sql, sql_template, health_status, current_timestamp, uuid);

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
											  char temp_buf[WIFI_MESSAGE_LENGTH];
    char *string_begin;
    char *string_end;
    int numbers = 0;
    char sql[SQL_TEMP_BUFFER_LENGTH];
    int ret_val = 0;
    char *sql_template = "INSERT INTO tracking_table " \
                         "(object_mac_address, " \
                         "lbeacon_uuid, " \
                         "initial_timestamp, " \
                         "final_timestamp) " \
                         "VALUES " \
                         "(\'%s\', \'%s\', to_timestamp(\'%s\'), to_timestamp(\'%s\'));";
	char *object_mac_address = NULL;
    char *lbeacon_uuid = NULL;
    char *initial_timestamp_GMT = NULL;
    char *final_timestamp_GMT = NULL;

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
        object_mac_address = string_end + 1;
        string_end = strstr(object_mac_address, DELIMITER_SEMICOLON);
        *string_end = '\0';

        lbeacon_uuid = string_end + 1;
        string_end = strstr(lbeacon_uuid, DELIMITER_SEMICOLON);
        *string_end = '\0';

		initial_timestamp_GMT = string_end + 1;
        string_end = strstr(initial_timestamp_GMT, DELIMITER_SEMICOLON);
        *string_end = '\0';

		final_timestamp_GMT = string_end + 1;
        string_end = strstr(final_timestamp_GMT, DELIMITER_SEMICOLON);
        *string_end = '\0';

        /* Create SQL statement */
        memset(sql, 0, sizeof(sql));
        sprintf(sql, sql_template, object_mac_address, lbeacon_uuid, initial_timestamp_GMT, final_timestamp_GMT);

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


