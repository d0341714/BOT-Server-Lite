#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "SqlWrapper.h"

static int SQL_callback(void *NotUsed,
                        int argc,
                        char **argv,
                        char **azColName){
    int i;
    for(i = 0; i < argc; i++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int SQL_execute(void* db, char* sql_statement){
    sqlite3 *sql_db = (sqlite3*)db;
    char *zErrMsg = NULL;
    int rc;

    /* Execute SQL statement */
    rc = sqlite3_exec(sql_db, sql_statement, SQL_callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
        sqlite3_free(zErrMsg);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
    }else{
        fprintf(stdout, "Records created successfully\n");
    }
}

int SQL_open_database_connection(char* db_filepath, void* db){
    sqlite3 *sql_db;
    int ret = sqlite3_open(db_filepath, &sql_db);

    if(ret){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }else{
        db = (void*)sql_db;
        fprintf(stderr, "Opened database successfully\n");
    }
    return ret;
}

int SQL_close_database_connection(void* db){
    sqlite3 *sql_db = (sqlite3*)db;
    sqlite3_close(sql_db);
}

int SQL_begin_transaction(void* db){
    int ret = 0;
    char *sql;

    /* Create SQL statement */
    sql = "BEGIN TRANSACTION;";

    /* Execute SQL statement */
    ret = SQL_execute(db, sql);

    return ret;
}

int SQL_end_transaction(void* db){
    int ret = 0;
    char *sql;

    /* Create SQL statement */
    sql = "END TRANSACTION;";

    /* Execute SQL statement */
    ret = SQL_execute(db, sql);

    return ret;
}

int SQL_update_gateway_registration_status(void* db,
                                           char* buf,
                                           size_t buf_len){
    SQL_begin_transaction(db);

    SQL_end_transaction(db);
}

int SQL_update_lbeacon_registration_status(void* db,
                                           char* buf,
                                           size_t buf_len){
    SQL_begin_transaction(db);

    SQL_end_transaction(db);
}

int SQL_query_registered_gateways(void* db,
                                  int health_status,
                                  char* output,
                                  size_t output_len){

}

int SQL_update_gateway_health_status(void* db,
                                     char* buf,
                                     size_t buf_len){
    SQL_begin_transaction(db);

    SQL_end_transaction(db);
}

int SQL_update_lbeacon_health_status(void* db,
                                     char* buf,
                                     size_t buf_len){

}

int SQL_update_object_tracking_data(void* db,
                                    char* buf,
                                    size_t buf_len){
    SQL_begin_transaction(db);

    SQL_end_transaction(db);
}

int SQL_update_object_geo_fencing_alert(void* db,
                                        char* buf,
                                        size_t buf_len){
    SQL_begin_transaction(db);

    SQL_end_transaction(db);
}
