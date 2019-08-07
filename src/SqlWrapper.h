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

     Chun-Yu Lai   , chunyu1202@gmail.com
 */

#ifndef SQL_WRAPPER_H
#define SQL_WRAPPER_H

#include "BeDIS.h"
#include <libpq-fe.h>

/* When debugging is needed */
//#define debugging

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

     gateway_ip_address - the real ip address of Gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_lbeacon_registration_status(void *db,
                                                 char *buf,
                                                 size_t buf_len,
                                                 char *gateway_ip_address);


/*
  SQL_update_gateway_health_status

     Updates the health status of the input gateways

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           health status of gateway

           gateway_ip;health_status;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of Gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_gateway_health_status(void *db,
                                           char *buf,
                                           size_t buf_len,
                                           char *gateway_ip_address);


/*
  SQL_update_lbeacon_health_status

     Updates the health status of the input lbeacons

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an input string with the format below to specify the
           health status of lbeacon

           lbeacon_uuid;lbeacon_datetime;lbeacon_ip;health_status;

     buf_len - Length in number of bytes of buf input string

     gateway_ip_address - the real ip address of Gateway

  Return Value:

     ErrorCode - indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY
*/
ErrorCode SQL_update_lbeacon_health_status(void *db,
                                           char *buf,
                                           size_t buf_len,
                                           char *gateway_ip_address);


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
  SQL_insert_geo_fence_alert

     This function insert received geo fence alert into geo_fence_alert table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     buf - pointer to an array to receive the returned geo fence information.
           The returned message format is below.

            number_of_geo_fence_alert;mac_address1;type1;uuid1;alert_time1;
            rssi1;mac_address2;type2;uuid2;alert_time2;rssi2;

     buf_len - Length in number of bytes of buf

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_insert_geo_fence_alert(void *db, char *buf, size_t buf_len);

/*
  SQL_summarize_object_inforamtion

     This function invokes sub-functions to summarize information of object 
     list for BOT sysmte GUI. The summary informtion is updated to the summary 
     table object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     location_time_interval_in_sec - the time window in which we treat this 
                                     object as shown and visiable by BOT system

     panic_time_interval_in_sec - the time window in which we treat this object 
                                  as in panic situation if object (user) presses 
                                  panic button within the interval.

     geo_fence_time_interval_in_sec - the time window in which we treat this 
                                      object as shown, visiable, and geofence 
                                      violation.
  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_summarize_object_inforamtion(void *db, 
                                           int location_time_interval_in_sec, 
                                           int panic_time_interval_in_sec,
                                           int geo_fence_time_interval_in_sec);

/*
  SQL_summarize_object_location

     This function queries tracking_table (Time-Series database) within time 
     window to find out the lbeacon_uuid with the strongest RSSI value for each
     object mac_address. It is also responsible for maintaining the first seen 
     timestamp and last seen timestamp of the object mac_address and 
     lbeacon_uuid pair. The summary information is updated to the summary table 
     object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     time_interval_in_sec - the time window in which we treat this object as 
                            shown and visiable by BOT system

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_summarize_object_location(void *db, int time_interval_in_sec);

/*
  SQL_identify_panic

     This function queries tracking_table (Time-Series database) within time 
     window to find out whether the object (user) has pressed panic_button 
     in the past time interval. The panic button status is updated to the 
     summary table object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     time_interval_in_sec - the time window in which we treat this object 
                            as in panic situation if object (user) presses 
                            panic button within the interval.

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_identify_panic(void *db, int time_interval_in_sec);

/*
  SQL_identify_geo_fence

     This function queries geo_fence_alert table within time window to find 
     out whether the object violated geo-fence in the past time interval. 
     The geo-fence status is updated to the summary table object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     time_interval_in_sec - the time window in which we treat this object 
                            as shown, visiable, and geofence violation.

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_identify_geo_fence(void *db, int time_interval_in_sec);


/*
  SQL_identify_last_activity_status

     This function use each pair of object mac_address and lbeacon_uuid from
     the summary table object_summary_table to check the activity status 
     records stored in tracking_table (Time-Series database). It uses the 
     features called TIME_BUCKET and Delta provided by timescaleDB (TSDB) to
     identify the activity status. The activity status is updated to the 
     summary table object_summary_table.

  Parameter:

     db - a pointer pointing to the connection to the database backend server

     time_interval_in_min - the time window in which we want to monitor the 
                            movement activity

     each_time_slot_in_min - the time slot in minutes in which we calculate 
                             the running average of RSSI for comparison

     rssi_delta - the delta value of RSSI which we used as a criteria to 
                  identify the movement of object

  Return Value:

     ErrorCode - Indicate the result of execution, the expected return code
                 is WORK_SUCCESSFULLY.
*/
ErrorCode SQL_identify_last_activity_status(void *db, 
                                            int time_interval_in_min, 
                                            int each_time_slot_in_min,
                                            unsigned int rssi_delta);

#endif