#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>


static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
   for(i=0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

int main(int argc, char* argv[])
{
   sqlite3 *db;
   int rc = 0;
   char *sql;
   char *zErrMsg;

   rc = sqlite3_open("bot_lite.db", &db);

   if( rc ){
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      exit(0);
   }else{
      fprintf(stderr, "Opened database successfully\n");
   }

   sql = "CREATE TABLE IF NOT EXISTS tracking_table("  \
         "id INTEGER PRIMARY KEY    AUTOINCREMENT," \
         "mac_address               NVARCHAR(17) NOT NULL," \
         "uuid                      NVARCHAR(32) NOT NULL," \
         "geo_fence_alert           TINYINT NOT NULL," \
         "intiial_timestamp_GMT     UNSIGNED BIG INT," \
         "current_timestamp_GMT     UNSIGNED BIT INT);";

   /* Execute SQL statement */

   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   if( rc != SQLITE_OK ){
   fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }else{
      fprintf(stdout, "Table created successfully\n");
   }

   sql = "CREATE TABLE IF NOT EXISTS gateway_table("  \
         "id INTEGER PRIMARY KEY     AUTOINCREMENT," \
         "ip_address                 NVARCHAR(15) NOT NULL UNIQUE," \
         "location_description       NVARCHAR(256)," \
         "health_status              INT NOT NULL," \
         "registered_timestamp_GMT   UNSIGNED BIT INT," \
         "last_report_timestamp_GMT  UNSIGNED BIT INT);";

   /* Execute SQL statement */

   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   if( rc != SQLITE_OK ){
   fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }else{
      fprintf(stdout, "Table created successfully\n");
   }

   sql = "CREATE TABLE IF NOT EXISTS lbeacon_table("  \
         "id INTEGER PRIMARY KEY     AUTOINCREMENT," \
         "uuid                       NVARCHAR(32) NOT NULL UNIQUE," \
         "location_description       NVARCHAR(256)," \
         "coordinates                NVARCHAR(18)," \
         "geo_fence_flag             TINYINT," \
         "health_status              INT NOT NULL," \
         "gateway_ip                 NVARCHAR(15)," \
         "registered_timestamp_GMT   UNSIGNED BIT INT," \
         "last_report_timestamp_GMT  UNSIGNED BIT INT);";

   /* Execute SQL statement */

   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   if( rc != SQLITE_OK ){
   fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }else{
      fprintf(stdout, "Table created successfully\n");
   }

   sql = "CREATE TABLE IF NOT EXISTS object_table("  \
         "id INTEGER PRIMARY KEY     AUTOINCREMENT," \
         "mac_address                NVARCHAR(17) NOT NULL UNIQUE," \
         "type                       INT NOT NULL," \
         "name                       NVARCHAR(32)," \
         "asset_owner_id             INT NOT NULL," \
         "available_status           INT NOT NULL," \
         "profile                    NVARCHAR(32)," \
         "user_id                    INT NOT NULL," \
         "registered_timestamp_GMT   UNSIGNED BIT INT);";

   /* Execute SQL statement */

   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   if( rc != SQLITE_OK ){
   fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }else{
      fprintf(stdout, "Table created successfully\n");
   }
  
   sqlite3_close(db);

   return 0;
}
