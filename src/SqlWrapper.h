#ifndef SQL_WRAPPER_H
#define SQL_WRAPPER_H

#include "BeDIS.h"

/* SQL_callback

      The callback function used by database operations

  Parameter:

      NotUsed - Not used
      argc - number of parameters
      argv - the Parameters
      azColName - column names

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
static int SQL_callback(void *NotUsed, int argc, char **argv, char **azColName);

/* SQL_execute

      The execution function used by database operations

  Parameter:

      db - the pointer pointing to database
      sql_statement - The SQL statement to be executed

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
static int SQL_execute(void* db, char* sql_statement);

/* SQL_begin_transaction

     Begin transaction of SQL database operations.

  Parameter:

      db - the pointer pointing to database

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
static int SQL_begin_transaction(void* db);

/* SQL_end_transaction

     End transaction of SQL database operations.

  Parameter:

      db - the pointer pointing to database

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
static int SQL_end_transaction(void* db);

/* SQL_rollback_transaction

    Rollback transaction of SQL database operations.

  Parameter:

      db - the pointer pointing to database

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
static int SQL_rollback_transaction(void* db);

/* SQL_open_database_connection

      Open the database connection for operations

  Parameter:

      db_filepath - the full file path of database file

      db - the pointer pointing to database

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
int SQL_open_database_connection(char* db_filepath, void** db);

/* SQL_close_database_connection

      Close the database connection

  Parameter:

      db - the pointer pointing to database

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
int SQL_close_database_connection(void* db);

/* SQL_update_gateway_registration_status

      Update the input gateways as registered.

  Parameter:

      db - the pointer pointing to database

      buf: an input string with the format below to specify the registered
           gateways

           length;gateway_ip_1;gateway_ip_2;gateway_ip_3;

      buf_len: Length in number of bytes of buf input string

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
int SQL_update_gateway_registration_status(void* db, char* buf, size_t buf_len);

/* SQL_update_lbeacon_registration_status

      Update the input lbeacons as registered.

  Parameter:

      db - the pointer pointing to database

      buf: an input string with the format below to specify the registered
           gateways.

           length;gateway_ip;uuid_1,registered_timestamp_GMT;uuid_2,\
           reigstered_timestamp_GMT;

      buf_len: Length in number of bytes of buf input string

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
int SQL_update_lbeacon_registration_status(void* db, char* buf, size_t buf_len);

/* SQL_query_registered_gateways

      Return all the gateways with the corresponding health_status

  Parameter:

      db - the pointer pointing to database

      health_status: the desired health_status
                     -1: all health_status
                     0: health_status is healthy
                     1: health_status is error
      output: an output buffer to receive the query resutls. The result string
              is in format below.

           length;health_status;gateway_ip_1;gateway_ip_2;gateway_ip_3;

      output_len: The maximum length in number of bytes of output buffer.

  Return Value:

      int: If return 0, everything work successful.
           If not 0, Something wrong.
*/
int SQL_query_registered_gateways(void* db,
                                  int health_status,
                                  char* output,
                                  size_t output_len);

/* SQL_update_gateway_health_status

      Update health status of gateways

  Parameter:

      db - the pointer pointing to database

      buf: an input string with the format below to specify the health status
           of gateways

           length;gateway_ip_1,health_status;gateway_ip_2,health_status;

      buf_len: Length in number of bytes of buf input string

  Return Value:

      int: If return 0, everything work successful.
       If not 0, Something wrong.
*/
int SQL_update_gateway_health_status(void* db, char* buf, size_t buf_len);

/* SQL_update_lbeacon_health_status

      Update health status of lbeacons

  Parameter:

      db - the pointer pointing to database

      buf: an input string with the format below to specify the health status
           of gateways

           length;lbeacon_uuid_1,health_status;lbeacon_uuid_2,health_status;

      buf_len: Length in number of bytes of buf input string

  Return Value:

      int: If return 0, everything work successful.
       If not 0, Something wrong.
*/
int SQL_update_lbeacon_health_status(void* db, char* buf, size_t buf_len);

/* SQL_update_object_tracking_data

      Update data of tracked objects

  Parameter:

      db - the pointer pointing to database

      buf: an input string with the format below to specify the health status
           of gateways

           length;object_mac_address_1,lbeacon_id_1,inital_timestamp_GMT_1,\
           current_timestamp_GMT_1;object_mac_address_2,lbeacon_id_2,\
           inital_timestamp_GMT_2,current_timestamp_GMT_2;

      buf_len: Length in number of bytes of buf input string

  Return Value:

      int: If return 0, everything work successful.
       If not 0, Something wrong.
*/
int SQL_update_object_tracking_data(void* db, char* buf, size_t buf_len);

/* SQL_update_object_geo_fencing_alert

      Specify the tracked objects as offending geo-fencing prohibition

  Parameter:

      db - the pointer pointing to database

      buf: an input string with the format below to specify the health status
           of gateways

           length;object_mac_address_1,object_mac_address_2;

      buf_len: Length in number of bytes of buf input string

  Return Value:

      int: If return 0, everything work successful.
       If not 0, Something wrong.
*/
int SQL_update_object_geo_fencing_alert(void* db, char* buf, size_t buf_len);

#endif
