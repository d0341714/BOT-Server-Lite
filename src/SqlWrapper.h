/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and conditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     sqlWrapper.h

  File Description:

     This file contains the header of  function declarations and variable used
     in sqlWrapper.c

  Version:

     1.0, 20190514

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

#ifndef SQL_WRAPPER_H
#define SQL_WRAPPER_H

#include "BeDIS.h"
#include <libpq-fe.h>

/* When debugging is needed */
//#define debugging

/* db lock */
pthread_mutex_t db_lock;

/*
  SQL_execute

     The execution function used by database operations

  Parameter:

     db - a pointer pointing to the connection to the database backend server
     sql_statement - The SQL statement to be executed

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
static ErrorCode SQL_execute(void *db, char *sql_statement);


/*
  SQL_begin_transaction

     Begins transaction makes the operations specified by the follows SQL
     database statement a transaction and holds a lock on the table until
     the transaction either is committed or rollbed back. Begin transaction
     makes the starting boundary of a transaction.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
static ErrorCode SQL_begin_transaction(void *db);


/*
  SQL_commit_transaction

     Commits the transaction marked by a previous begin transaction.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
static ErrorCode SQL_commit_transaction(void *db);


/*
  SQL_rollback_transaction

     Rolls back the transaction marked by a previous begine transaction.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
static ErrorCode SQL_rollback_transaction(void *db);


/*
  SQL_open_database_connection

     Opens a database connection to the database backend server

  Parameter:

     db_filepath - the full file path of database file

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_open_database_connection(char *db_filepath, void **db);


/*
  SQL_close_database_connection

     Closes a database connection

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_close_database_connection(void *db);


/*
  SQL_vacuum_database();

     Identifies space occupied by deleted rows and catelogues to be garbage
     collected.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_vacuum_database(void *db);


/*
  SQL_retain_data();

     Deletes the rows and catelogues which are older than the specified number
     of hours ago.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     retention_hours - specify the hours for data retention

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_retain_data(void *db, int retention_hours);


/*
  SQL_update_gateway_registration_status

     Updates the input gateways as registered.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - an input string with the format below to specify the registered
           gateways

           length;gateway_ip_1;gateway_ip_2;gateway_ip_3;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_gateway_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len);


/*
  SQL_query_registered_gateways

     Returns all the gateways with the corresponding health_status

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     health_status - the health_status to be queried

     output - pointer to the output buffer to receive the query resutls. The
              result string is in the format below.

              length;gateway_ip_1;health_status_1;gateway_ip_2; \
              health_status_2;gateway_ip_3;health_status_3;

     output_len - The maximum length in number of bytes of output buffer.

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_query_registered_gateways(void *db,
                                        HealthStatus health_status,
                                        char *output,
                                        size_t output_len);


/*
  SQL_update_lbeacon_registration_status

     Updates the status of the input lbeacons as registered.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           registered gateways.

           length;gateway_ip;lbeacon_uuid_1;lbeacon_registered_timestamp_GMT; \
           lbeacon_ip_1;lbeacon_uuid_2;lbeacon_reigstered_timestamp_GMT; \
           lbeacon_ip_2;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_lbeacon_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len);


/*
  SQL_update_gateway_health_status

     Updates the health status of the input gateways

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           health status of gateways

           length;gateway_ip_1;health_status;gateway_ip_2;health_status;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_gateway_health_status(void *db,
                                           char *buf,
                                           size_t buf_len);


/*
  SQL_update_lbeacon_health_status

     Updates the health status of the input lbeacons

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           health status of gateways

           length;lbeacon_uuid_1;gateway_ip;health_status_1; \
           lbeacon_uuid_2;gateway_ip;health_status_2;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_lbeacon_health_status(void *db,
                                           char *buf,
                                           size_t buf_len);


/*
  SQL_update_object_tracking_data

     Updates data of tracked objects

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           lbeacon_uuid;lbeacon_datetime;lbeacon_ip;object_type; \
           object_number;object_mac_address_1;initial_timestamp_GMT_1; \
           final_timestamp_GMT_1;rssi_1;push_button_1;object_type; \
           object_number;object_mac_address_2;initial_timestamp_GMT_2; \
           final_timestamp_GMT_2;rssi_2;push_button_2;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_update_object_tracking_data(void *db,
                                          char *buf,
                                          size_t buf_len);


/*
  SQL_update_api_data_owner

     Updates the data owner for the BOT system to maintain and check whether the
     provider is still alive.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           topic_name;ip_address;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     int - If the data owner update successfully, it will return it's id in the
           database. If failed, it will return -1.
*/
int SQL_update_api_data_owner(void *db, char *buf, size_t buf_len);


/*
  SQL_remove_api_data_owner

     Remove the data owner when the provider not alive or the provider send the
     request to remove the topic.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           topic_id;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_remove_api_data_owner(void *db, char *buf, size_t buf_len);


/*
  SQL_get_api_data_owner_id

     This function uses name to get the id of the data in the database.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           topic_name;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     int - If the function executed successfully, it will return it's id of the
           data owner. If failed, it will return -1.
*/
int SQL_get_api_data_owner_id(void *db, char *buf, size_t buf_len);


/*
  SQL_update_api_subscription

     Updates the subscriber for the BOT system to maintain and check where the
     message needs to send when the data update.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           topic_id;ip_address;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     int - If the topic update successfully, it will return it's id of the
           subscriber. If failed, it will return -1.
*/
int SQL_update_api_subscription(void *db, char *buf, size_t buf_len);


/*
  SQL_remove_api_subscription

     Remove the subscriber when the subscriber do not want get the message when
     the data update.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below.

           subscriber_id;

     buf_len - Length in number of bytes of buf input string

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_remove_api_subscription(void *db, char *buf, size_t buf_len);


/*
  SQL_get_api_subscribers

     This function uses data owner id to get subscribers.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - The pointer points to an input string with the format below. After
           collecting the ip address of subscribers, it will use for storing the
           string of the list of the ip address

           data_owner_id;

     buf_len - Length in number of bytes of the buf string

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_get_api_subscribers(void *db, char *buf, size_t buf_len);


/*
  SQL_get_geo_fence

     This function get geo fence table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an array to receive the returned geo fence information.
           The returned message format is below.

            number_of_geo_fence;id;name;number_of_perimeters,perimeters_lbeacon1
            ,throshold1, ...;number_of_fence,fence_lbeacon1,throshold1,...;
            number_of_mac_prefix,mac_prefix1,...;

     buf_len - Length in number of bytes of buf

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_get_geo_fence(void *db, char *buf);


/*
  SQL_insert_geo_fence_alert

     This function insert received geo fence alert into geo_fence_alert table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an array to receive the returned geo fence information.
           The returned message format is below.

            number_of_geo_fence_alert;mac_address1;type1;uuid1;alert_time1;
            rssi1;geo_fence_id1;mac_address2;type2;uuid2;alert_time2;
            rssi2;geo_fence_id2;

     buf_len - Length in number of bytes of buf

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_insert_geo_fence_alert(void *db, char *buf, size_t buf_len);


#endif
